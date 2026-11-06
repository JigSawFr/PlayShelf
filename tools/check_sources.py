#!/usr/bin/env python3
"""Static checks on the layout of source/ (run in CI and by `make check`).

Errors:
- the kit (source/kit) including the app: a file of the kit includes only
  the kit, the standard library, Aurore, boreale-link and the vendored
  libraries, plus the app's identity and settings (APP_CONTRACT below); it
  asks the app's
  screens through ui::app_hooks (ui/app_hooks.hpp) or Aurore's ui::hooks. The kit is what PlayShelf shares
  with PlayGuard: it must stay ready to become a library of its own;
- an include going up the layers: ui/, view/ and util/ (the kit's and the
  app's) never include activity/ or tab/; a flow of action/ includes only
  the screens it opens itself, listed below with why;
- library/aurore pointing at a commit that is no `vX.Y.Z` tag of Aurore (an
  app follows a release, never a commit of main). Skipped, with a note, when
  the tags cannot be listed (no network);

Usage: tools/check_sources.py [repo-root]
"""
import glob
import os
import re
import subprocess
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
# The kit's other half is the submodule library/boreale-kit (its own CI checks it).
kit_files = {rel[len("source/kit/"):] for rel, _ in sources("source/kit")} | \
            {rel[len("library/boreale-kit/source/"):] for rel, _ in sources("library/boreale-kit/source")}
# boreale-link (library/boreale-link), the C layer the kit stands on, as on Aurore.
link_files = set(os.listdir(os.path.join(ROOT, "library/boreale-link/source"))) \
    if os.path.isdir(os.path.join(ROOT, "library/boreale-link/source")) else set()
for rel, text in sources("source/kit"):
    here = os.path.dirname(rel[len("source/kit/"):])
    for inc in INCLUDE.findall(text):
        if inc in APP_CONTRACT or inc in kit_files or inc in link_files:
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
LOWER = ["source/kit/ui", "source/kit/view", "source/ui", "source/view", "source/util"]
UPWARD = re.compile(r'^\s*#\s*include\s+"((?:activity|tab)/[^"]+)"', re.M)

# A flow that opens its own screen includes it (the flow is the screen's way
# in); nothing else of action/ includes a screen.
ACTION_SCREENS = {
    "source/action/library_flow.cpp": {"activity/server_activity.hpp"},             # a library server's editor
    "source/action/game_page.cpp": {"activity/game_activity.hpp"},                  # a game's page
}

for folder in LOWER:
    for rel, text in sources(folder):
        for inc in UPWARD.findall(text):
            errors.append(f'{rel} includes "{inc}": {folder}/ is below the screens (ask them through ui/app_hooks.hpp)')
seen = set()
for folder in ("source/action",):
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

def read(path):
    try:
        return open(f"{ROOT}/{path}", encoding="utf-8").read()
    except OSError:
        errors.append(f"{path} not found")
        return ""


# ------------------------------------------------------------ aurore's tag
def git(*args):
    return subprocess.run(["git", "-C", ROOT, *args], capture_output=True, text=True, timeout=60)


def check_aurore_tag():
    pin = git("ls-tree", "HEAD", "library/aurore")
    fields = pin.stdout.split()
    if pin.returncode != 0 or len(fields) < 3 or fields[0] != "160000":
        return  # not a git checkout, or no such submodule
    sha = fields[2]
    url = git("config", "-f", ".gitmodules", "--get", "submodule.library/aurore.url").stdout.strip()
    if not url:
        errors.append(".gitmodules has no submodule.library/aurore.url")
        return
    try:
        tags = subprocess.run(["git", "ls-remote", "--tags", url], capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        tags = None
    if tags is None or tags.returncode != 0:
        print("sources check: Aurore's tags could not be listed, library/aurore's pin not checked")
        return
    release = re.compile(r"^refs/tags/v\d+\.\d+\.\d+(\^\{\})?$")
    if not any(line.split()[0] == sha for line in tags.stdout.splitlines() if release.match(line.split()[1])):
        errors.append(f"library/aurore points at {sha[:12]}, which is no vX.Y.Z tag of Aurore: "
                      "an app follows a release of Aurore, never a commit of main")


check_aurore_tag()

for e in errors:
    print("ERROR:", e)
print(f"sources check: {len(errors)} error(s)")
sys.exit(1 if errors else 0)
