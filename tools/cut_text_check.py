#!/usr/bin/env python3
"""Checks that no text of the main screen's tabs, of a few common dialogs and
of the child screen is cut short ("…"), in the console's 15 languages, at the
normal text size and the larger one.

For each language and size, starts the desktop build (build-desktop/playshelf)
on the tabs, shows every tab of the rail, then, from Today, Stop for today:
its picker, the confirmation of the unlock it needs and the failure dialog
of that unlock (PLAYSHELF_SIM_FAIL=unlock); then Show the child. It reads
what PLAYSHELF_SIM_CUT_TEXT=1 gathered in the run folder's cut_text.txt
(source/ui/cut_text.cpp: every label its box cuts, by screen; "dashboard ›
dialog" for a dialog over Today). A cut not listed in
tests/cut_text/allowed.txt fails the check; a listed one that no longer
happens is reported (the line can go).

Usage: tools/cut_text_check.py <out-dir> [language ...]
       (needs DISPLAY and xdotool: xvfb-run in CI)

tests/cut_text/allowed.txt: one cut a line, "<language> <size> <screen> <text>"
separated by tabs, "*" for any language or size; "#" starts a comment.
"""
import os
import re
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(ROOT, "build-desktop", "playshelf")
ALLOWED = os.path.join(ROOT, "tests", "cut_text", "allowed.txt")
SIZES = ("normal", "large")
TABS = 9                 # the rail: Today … About
TAB_WAIT_S = 1.3         # a tab drawn, then a sample (once a second)
# The failure dialog's line in the log (ui::error), whatever the language:
# the simulated service's refusal, as rc_text gives it.
FAILURE_LOGGED = "0x0000C88E"


def languages():
    with open(os.path.join(ROOT, "source", "util", "config.hpp"), encoding="utf-8") as f:
        m = re.search(r"LANGUAGES\[\]\s*=\s*\{([^}]*)\}", f.read())
    return [l for l in re.findall(r'"([^"]+)"', m.group(1)) if l != "system"]


def allowed():
    rules = []
    if os.path.exists(ALLOWED):
        with open(ALLOWED, encoding="utf-8") as f:
            for line in f:
                line = line.rstrip("\n")
                if not line.strip() or line.lstrip().startswith("#"):
                    continue
                parts = line.split("\t")
                if len(parts) != 4:
                    sys.exit(f"{ALLOWED}: not 4 tab-separated fields: {line!r}")
                rules.append(tuple(parts))
    return rules


def matches(rule, cut):
    return all(r in ("*", c) for r, c in zip(rule, cut))


def key(name):
    subprocess.run(["xdotool", "key", "--delay", "60", name], check=False)


# A press held this long, then this long for the screen to follow: under
# software GL on a busy machine, a press shorter than a frame can be missed.
HOLD_S = 0.06
MOVE_S = 0.35


def press(name):
    subprocess.run(["xdotool", "keydown", name, "sleep", str(HOLD_S), "keyup", name], check=False)
    time.sleep(MOVE_S)


def logged(log, text, seconds):
    """Whether `text` reaches the app's log within `seconds`."""
    end = time.time() + seconds
    while True:
        with open(log, encoding="utf-8", errors="replace") as lf:
            if text in lf.read():
                return True
        if time.time() > end:
            return False
        time.sleep(0.25)


def run(out, lang, size):
    """The cuts of one language at one size: (lang, size, screen, text)."""
    base = os.path.join(out, f"{lang}-{size}")
    shutil.rmtree(base, ignore_errors=True)
    data = os.path.join(base, "run", "playshelf_data")
    os.makedirs(data)
    with open(os.path.join(data, "config.json"), "w") as f:
        f.write('{"schema": 1, "language": "%s", "theme": "dark", "text_size": "%s"}'
                % (lang, size))
    cuts = os.path.join(base, "run", "cut_text.txt")   # the app's working directory
    log = os.path.join(base, "app.log")
    env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE="1", TZ="UTC", PLAYSHELF_SIM_NOW="1791475200",
               PLAYSHELF_SIM_STILL_FOCUS="1", PLAYSHELF_SIM_VERSION="1.0.0:1a2b3c4",
               PLAYSHELF_SIM_CHECK_MS="0", PLAYSHELF_SIM_CUT_TEXT="1",
               # The unlock Stop for today needs fails: its failure dialog.
               PLAYSHELF_SIM_FAIL="unlock")
    with open(log, "w") as lf:
        # Line-buffered, so "first screen ready" reaches the log at once; in
        # a D-Bus session of its own, as tools/desktop_smoke.py starts it.
        cmd = (["stdbuf", "-oL", "-eL"] if shutil.which("stdbuf") else []) + [APP]
        if shutil.which("dbus-run-session"):
            cmd = ["dbus-run-session", "--"] + cmd
        app = subprocess.Popen(cmd, cwd=os.path.join(base, "run"), env=env, stdout=lf, stderr=subprocess.STDOUT)
    try:
        for _ in range(60):
            time.sleep(0.5)
            if app.poll() is not None:
                sys.exit(f"{lang} {size}: the app exited at start-up (see {log})")
            with open(log, encoding="utf-8", errors="replace") as lf:
                if "first screen ready" in lf.read():
                    break
        else:
            sys.exit(f"{lang} {size}: no first screen in 30 s (see {log})")
        time.sleep(TAB_WAIT_S)
        for _ in range(TABS - 1):
            key("Down")
            time.sleep(TAB_WAIT_S)
        # Today's hero: Extra time, then Stop for today below it.
        for name in ["Up"] * (TABS - 1) + ["Right", "Down", "Return"]:
            press(name)
        time.sleep(TAB_WAIT_S)          # its picker: now, or in 5 minutes
        press("Return")                 # now
        time.sleep(TAB_WAIT_S)          # the confirmation, with the unlock it needs
        press("Right")
        press("Return")                 # Unlock and apply: the unlock fails
        if not logged(log, FAILURE_LOGGED, 10):
            sys.exit(f"{lang} {size}: the failure dialog did not open (see {log})")
        time.sleep(TAB_WAIT_S)          # the failure dialog
        press("Return")                 # OK
        press("Right")                  # Show the child
        press("Return")
        time.sleep(TAB_WAIT_S + 0.5)    # its entrance, then a sample
    finally:
        app.terminate()
        try:
            app.wait(5)
        except subprocess.TimeoutExpired:
            app.kill()
    found = []
    if os.path.exists(cuts):
        with open(cuts, encoding="utf-8") as f:
            for line in f:
                screen, _, text = line.rstrip("\n").partition("\t")
                found.append((lang, size, screen, text))
    return found


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = os.path.abspath(sys.argv[1])
    langs = sys.argv[2:] or languages()
    rules = allowed()
    used = set()
    new = []
    for lang in langs:
        for size in SIZES:
            cuts = run(out, lang, size)
            for cut in cuts:
                hit = [r for r in rules if matches(r, cut)]
                used.update(hit)
                if not hit:
                    new.append(cut)
            print(f"{lang} {size}: {len(cuts)} cut")
    # A "*" rule only when every language ran (CI splits them in shards).
    every = set(langs) >= set(languages())
    for rule in rules:
        if rule not in used and ((rule[0] == "*" and every) or rule[0] in langs):
            print(f"note: no longer cut, the line can go: {chr(9).join(rule)}")
    if new:
        print(f"{len(new)} text(s) cut short (add a line to tests/cut_text/allowed.txt only if it is meant):")
        for cut in new:
            print("\t".join(cut))
        return 1
    print(f"cut text check: {len(langs)} languages, {len(SIZES)} sizes, nothing cut that is not allowed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
