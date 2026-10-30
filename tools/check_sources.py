#!/usr/bin/env python3
"""Static checks on the layout of source/ (run in CI and by `make check`).

Errors:
- the kit (source/kit) including the app: a file of the kit includes only
  the kit, the standard library, Aurore and the vendored libraries, plus
  the app's identity and settings (APP_CONTRACT below); it asks the app's
  screens through ui::app_hooks (ui/app_hooks.hpp) or Aurore's ui::hooks. The kit is what PlayShelf shares
  with PlayGuard: it must stay ready to become a library of its own;
- an include going up the layers: ui/, view/ and util/ (the kit's and the
  app's) never include activity/ or tab/; a flow of action/ includes only
  the screens it opens itself, listed below with why;
- a C file of source/kit/core that needs no console service (every one but
  the CONSOLE_ONLY ones below) missing from the desktop build's list
  (CMakeLists.txt, which leaves source/kit/core/*.c out and adds those back
  by hand: the simulator shares them) or from the host tests' sources
  (Makefile); a name below that is no file any more.

Usage: tools/check_sources.py [repo-root]
"""
import glob
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
errors = []

INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def sources(folder):
    for path in sorted(glob.glob(f"{ROOT}/{folder}/**/*", recursive=True)):
        if path.endswith((".c", ".cpp", ".h", ".hpp")):
            yield os.path.relpath(path, ROOT).replace(os.sep, "/"), open(path, encoding="utf-8").read()


# ------------------------------------------------------------ the kit
# What the kit may include of the app: its identity (names, version, modes, links)
# and its settings. Everything else of the app reaches the kit through
# ui::hooks.
APP_CONTRACT = {"app.hpp", "app_id.h", "util/config.hpp"}
kit_files = {rel[len("source/kit/"):] for rel, _ in sources("source/kit")}
for rel, text in sources("source/kit"):
    here = os.path.dirname(rel[len("source/kit/"):])
    for inc in INCLUDE.findall(text):
        if inc in APP_CONTRACT or inc in kit_files:
            continue
        # A relative include ("../core/x.h", "sim_state.h") inside the kit.
        if os.path.normpath(os.path.join(here, inc)) in kit_files:
            continue
        if inc.startswith(("Aurore", "nanovg", "yoga", "fmt/")) or inc in ("qrcodegen.hpp", "check.h"):
            continue
        errors.append(f'{rel} includes "{inc}", which is the app\'s: the kit includes only itself, '
                      f'{" and ".join(sorted(APP_CONTRACT))} (ask the app through ui/app_hooks.hpp)')

# --------------------------------------------------------------- layers
# Below the screens: never activity/ nor tab/.
LOWER = ["source/kit/ui", "source/kit/view", "source/kit/util", "source/ui", "source/view", "source/util"]
UPWARD = re.compile(r'^\s*#\s*include\s+"((?:activity|tab)/[^"]+)"', re.M)

# A flow that opens its own screen includes it (the flow is the screen's way
# in); nothing else of action/ includes a screen.
ACTION_SCREENS = {
    "source/kit/action/dev_build_flow.cpp": {"activity/dev_build_activity.hpp",     # the development builds' screens
                                             "activity/dev_builds_activity.hpp"},
    "source/kit/action/support_flow.cpp": {"activity/whats_new_activity.hpp"},      # What's new
    "source/kit/action/update_flow.cpp": {"activity/update_activity.hpp"},          # the update screen
    "source/action/library_flow.cpp": {"activity/server_activity.hpp"},             # a library server's editor
    "source/action/game_page.cpp": {"activity/game_activity.hpp"},                  # a game's page
}

for folder in LOWER:
    for rel, text in sources(folder):
        for inc in UPWARD.findall(text):
            errors.append(f'{rel} includes "{inc}": {folder}/ is below the screens (ask them through ui/app_hooks.hpp)')
seen = set()
for folder in ("source/kit/action", "source/action"):
    for rel, text in sources(folder):
        allowed = ACTION_SCREENS.get(rel, set())
        for inc in UPWARD.findall(text):
            seen.add((rel, inc))
            if inc not in allowed:
                errors.append(f'{rel} includes "{inc}": a flow includes only a screen it opens '
                              "(else through ui/app_hooks.hpp; or list it in tools/check_sources.py, ACTION_SCREENS)")
for rel, allowed in sorted(ACTION_SCREENS.items()):
    for inc in sorted(allowed - {i for r, i in seen if r == rel}):
        errors.append(f'tools/check_sources.py: ACTION_SCREENS lists "{inc}" for {rel}, which does not include it')

# ------------------------------------------------------- source/kit/core
# The console's own: the simulator stands in for them on the desktop
# (source/kit/sim/), or the desktop has no use for them.
CONSOLE_ONLY = {
    "crash_handler.c": "the console's exception handler",
    "device_key.c": "sim_device_key.cpp",
    "local_time.c": "sim_local_time.c",
    "nacp.c": "a homebrew's control data, read on the console only for now",
    "nacp_read.c": "the same",
    "platform.c": "sim_platform.cpp",
    "startup_hook.c": "libnx's userAppInit",
    "sysinfo.c": "sim_sysinfo.c",
}
# Console-free, but neither in the desktop build nor host-tested yet.
NOT_ON_DESKTOP = {}


def read(path):
    try:
        return open(f"{ROOT}/{path}", encoding="utf-8").read()
    except OSError:
        errors.append(f"{path} not found")
        return ""


cmake = set(re.findall(r"\$\{CMAKE_CURRENT_SOURCE_DIR\}/source/kit/core/(\w+\.c)\b", read("CMakeLists.txt")))
make = set(re.findall(r"\b(?:source/kit|\$\(K\))/core/(\w+\.c)\b", read("Makefile")))
core = {os.path.basename(p) for p in glob.glob(f"{ROOT}/source/kit/core/*.c")}
if not cmake or not make or not core:
    errors.append("no source/kit/core/*.c found in CMakeLists.txt, the Makefile or source/kit/core")
for name in sorted(set(CONSOLE_ONLY) | set(NOT_ON_DESKTOP)):
    if name not in core:
        errors.append(f"tools/check_sources.py names source/kit/core/{name}, which is not there any more")
for name in sorted(core - set(CONSOLE_ONLY)):
    if name not in NOT_ON_DESKTOP and name not in cmake:
        errors.append(f"source/kit/core/{name} needs no console service but the desktop build leaves it out: add "
                      "it to CMakeLists.txt's console-free list (or, if the console needs it, to CONSOLE_ONLY in "
                      "tools/check_sources.py)")
    if name not in NOT_ON_DESKTOP and name not in make:
        errors.append(f"source/kit/core/{name} needs no console service but no host test builds it (Makefile)")
for name in sorted(cmake & set(CONSOLE_ONLY)):
    errors.append(f"CMakeLists.txt adds source/kit/core/{name} to the desktop build, which "
                  "tools/check_sources.py says needs the console (CONSOLE_ONLY)")

for e in errors:
    print("ERROR:", e)
print(f"sources check: {len(errors)} error(s)")
sys.exit(1 if errors else 0)
