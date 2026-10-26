#!/usr/bin/env python3
"""Headless UI smoke test of the desktop build (simulated backend).

Starts build-desktop/playshelf on an X display (Xvfb), opens every tab, the
pickers of Preferences, the quick settings (−), About › Help & diagnostics
and, with the developer mode turned on by seven presses of Y on About ›
Version, the developer tools and the diagnostic report; it fails if the app
dies on the way (borealis throws on unknown XML attributes, missing views,
…). Screenshots of each screen are written to the output folder.

The "loading" scenario stops the start-up on the loading screen
(PLAYSHELF_SIM_HOLD_LOADING), at its last step, for its screenshot. Every
other scenario waits for the first screen after it, at least 2 s later (the
intro).

Usage: tools/desktop_smoke.py <out-dir> [loading]   (needs DISPLAY, xdotool, ImageMagick)
Environment knobs of the simulated backend (PLAYSHELF_SIM_*) are passed
through; the console time is fixed (PLAYSHELF_SIM_NOW, TZ), the focus glow
still (PLAYSHELF_SIM_STILL_FOCUS) and the header's version badge on one
development build (PLAYSHELF_SIM_VERSION) unless set.
"""
import glob
import os
import re
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(ROOT, "build-desktop", "playshelf")
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "smoke")
SCENARIO = sys.argv[2] if len(sys.argv) > 2 else ""
LOADING = SCENARIO == "loading"
if SCENARIO not in ("", "loading"):
    print("unknown scenario:", SCENARIO)
    sys.exit(2)
os.makedirs(OUT, exist_ok=True)
run_dir = os.path.join(OUT, "run")
shutil.rmtree(run_dir, ignore_errors=True)   # a first start: no settings, no logs
os.makedirs(run_dir, exist_ok=True)

env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE="1")
# ImageMagick 7 keeps the version 6 commands (import, convert, montage) as
# aliases of `magick` that warn on every call; version 6 has no `magick`.
IM = ["magick"] if shutil.which("magick") else []
# One console time and time zone for every run, so the screenshots can be
# compared with the reference ones (tools/visual_check.py): Thursday
# 8 October 2026, 16:00 UTC. Only the footer clock follows the host.
env.setdefault("PLAYSHELF_SIM_NOW", "1791475200")
env.setdefault("TZ", "UTC")
# The focus highlight without its moving glow (ui/cadran_frame.cpp).
env.setdefault("PLAYSHELF_SIM_STILL_FOCUS", "1")
env["PLAYSHELF_SIM_FOCUS_LOG"] = "1"     # each line focused, by its words (go_to())
# One made-up development build, not this build's version and commit
# (app.cpp): the header's version badge stays the same from one build to the
# next.
env.setdefault("PLAYSHELF_SIM_VERSION", "1.0.0:1a2b3c4")
# No network: the update check answers at once.
env.setdefault("PLAYSHELF_SIM_LATEST", "offline")
if LOADING:
    env["PLAYSHELF_SIM_HOLD_LOADING"] = "1"

log = open(os.path.join(OUT, "app.log"), "w")


def have(tool):
    return shutil.which(tool) is not None


# Line-buffered output, so app.log is complete at any moment.
cmd = (["stdbuf", "-oL", "-eL"] if have("stdbuf") else []) + [APP]
if have("dbus-run-session"):
    cmd = ["dbus-run-session", "--"] + cmd
proc = subprocess.Popen(cmd, cwd=run_dir, env=env, stdout=log, stderr=subprocess.STDOUT)


def alive():
    return proc.poll() is None


# The pauses around a press and before a screenshot. borealis' longest
# animation (a toast coming in) takes 300 ms, and a label starts scrolling
# after 1.5 s focused: a screenshot comes 0.9 s after the last press, every
# animation over, no label moving yet.
KEY_BEFORE = 0.35
KEY_AFTER = 0.1
SHOT_WAIT = 0.45


def key(name, n=1, hold=0.02):
    """Presses `name` n times, each exactly once, waiting for the screen to
    settle before each press (borealis' animations last 300 ms at most) and
    releasing it `hold` seconds later, both sent by one xdotool process so a
    busy runner cannot delay the release."""
    for _ in range(n):
        time.sleep(KEY_BEFORE)
        subprocess.run(["xdotool", "keydown", name, "sleep", str(hold), "keyup", name], env=env)
        time.sleep(KEY_AFTER)


def log_lines():
    log.flush()
    return open(os.path.join(OUT, "app.log"), errors="replace").read().splitlines()


def focused():
    """The words of the line that has the focus now (PLAYSHELF_SIM_FOCUS_LOG:
    Aurore's cadran/ui/focus_log.cpp), or None."""
    last = None
    for l in log_lines():
        at = l.find("focus: ")
        if at >= 0:
            last = l[at + len("focus: "):]
    return last.replace(" ", " ") if last is not None else None


def go_to(label, direction="Down", most=25):
    """Presses `direction` until the line `label` has the focus (its words, in
    English), `most` times at most."""
    time.sleep(KEY_BEFORE)
    for _ in range(most):
        if focused() == label:
            return
        key(direction)
    if focused() != label:
        fail(f"no line {label!r} reached with {direction} (the focus on {focused()!r})")


def shot(name):
    time.sleep(SHOT_WAIT)
    subprocess.run(IM + ["import", "-window", "root", os.path.join(OUT, name + ".png")], env=env)
    if not alive():
        fail(f"app exited while showing {name}")


def logged(text):
    return any(text in l for l in log_lines())


def fail(msg):
    print("SMOKE FAILED:", msg)
    print("\n".join(log_lines()[-60:]))
    if alive():
        proc.kill()
    sys.exit(1)


def contact_sheet():
    """Every screen of the run on one image, contact-sheet.png, each under its
    name: a whole scenario can be looked over at a glance (CI keeps it with
    the screenshots). Not a screen: tools/visual_check.py only reads the
    names of the references."""
    shots = sorted(p for p in glob.glob(os.path.join(OUT, "*.png")) if not p.endswith("contact-sheet.png"))
    if not shots:
        return
    subprocess.run(IM + ["montage", "-label", "%t", "-background", "#1f1f1f", "-fill", "white",
                         "-tile", "4x", "-geometry", "320x180+6+6"] + shots + [os.path.join(OUT, "contact-sheet.png")],
                   env=env, capture_output=True)


def finish():
    proc.terminate()
    try:
        proc.wait(5)
    except subprocess.TimeoutExpired:
        proc.kill()
    contact_sheet()
    print("desktop smoke test passed:", OUT)
    sys.exit(0)


# The window, then its first frame drawn (not all black).
for _ in range(60):
    if subprocess.run(["xdotool", "search", "--name", "PlayShelf"], env=env, capture_output=True).returncode == 0:
        break
    if not alive():
        fail("app exited during start-up")
    time.sleep(0.5)
else:
    fail("no window after 30 s")
probe = os.path.join(OUT, "_first_frame.png")
for _ in range(40):
    subprocess.run(IM + ["import", "-window", "root", probe], env=env)
    mean = subprocess.run(IM + ["convert", probe, "-format", "%[fx:mean]", "info:"], capture_output=True, text=True).stdout
    if mean and float(mean) > 0.05:
        break
    if not alive():
        fail("app exited during start-up")
    time.sleep(0.5)
else:
    fail("the window stayed black for 20 s")
os.remove(probe)

# What is drawn first is the loading screen: wait for the screen after it
# (or, holding it, for its last step).
started = "start-up: held on the loading screen" if LOADING else "start-up: first screen ready"
for _ in range(60):
    if logged(started):
        break
    if not alive():
        fail("app exited during start-up")
    time.sleep(0.5)
else:
    fail("no first screen after the loading screen in 30 s")
time.sleep(2)
if not logged("start-up: intro, 2000 ms at least"):
    fail("no intro at start-up")

if LOADING:
    shot("01_loading")
    finish()

ready = [l for l in log_lines() if "start-up: first screen ready after" in l]
m = re.search(r"after (\d+) ms", ready[0]) if ready else None
if not m or int(m.group(1)) < 2000:
    fail("the first screen came before the end of the intro: " + (ready[0].strip() if ready else "no line"))

# The tabs, down the rail: Library, Catalogue, Downloads | Preferences, About.
shot("01_library")
key("Down")
shot("12_catalogue")
key("Down")
shot("11_downloads")
key("Down")
shot("02_preferences")
key("Right")                 # into Preferences: the language tile
key("Return")
shot("03_language_picker")
key("Escape")
go_to("Theme", "Right")
key("Return")
shot("04_theme_picker")
key("Down")                  # Light
key("Return")
shot("05_light_theme")
key("Return")                # the picker again: back to Dark
key("Down", 2)
key("Return")
key("Left", 3)
key("Left")                  # back to the rail
key("Down")                  # About
shot("06_about")
# The quick settings (−): the language, the theme, the text size.
key("minus")
shot("07_quick_settings")
key("Escape")
# Seven presses of Y on About › Version turn the developer mode on.
key("Right")
go_to("Version", "Up")
key("y", 7)
if not logged("developer mode: on"):
    fail("seven presses of Y on Version did not turn the developer mode on")
shot("08_dev_enabled")
go_to("Help & diagnostics")
key("Return")
shot("09_help")
go_to("Show the diagnostic report")
key("Return")
time.sleep(1)
shot("10_dev_report")
if not logged("main screen opened"):
    fail("the main screen was never opened")
finish()
