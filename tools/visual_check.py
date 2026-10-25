#!/usr/bin/env python3
"""Visual regression check of the desktop smoke test's screenshots.

Compares every reference image of <reference-dir> (tests/visual/<scenario>/
NAME.png) with NAME.png in <shots-dir>, the output folder of
tools/desktop_smoke.py, and fails when a screen changed. The smoke test fixes
the console's time (PLAYSHELF_SIM_NOW, TZ), so only the footer clock follows
the host (borealis reads it): it is redrawn at the console's time, 16:00,
in the clock's own font, colours and place, in the references as in the
screenshots compared with them. A pixel counts as changed when its colour moves by
more than --fuzz (15 %: anti-aliasing noise stays below that; the smoke test draws
the focus highlight without its moving glow, PLAYSHELF_SIM_STILL_FOCUS);
a screen fails when more than --max-pixels pixels change (100: two runs differ
by a few dozen at most, a value going from "2 h" to "2 h 1" by about 200). For
each failing screen, <shots-dir>/visual_diff/NAME.png shows the reference, the
new screenshot and the changes (in red) side by side.

The references are main's own screens (tools/visual_refs.py: the last run
of main keeps them, a pull request compares with them), never files kept
by hand. --screens FILE names the screens compared, one a line
(tests/visual/<scenario>/SCREENS): only screens that come out the same
from one run to the next, no toast, no held-key scrolling that can stop
anywhere (held to the end of a list is fine), no app version (a release
would change it), no SD-card content that piles up between local runs.
A screen of the list without a reference yet is new: reported, not failed.
Without --screens, every PNG of <reference-dir> is a reference (the web
simulator's check against main's site).

A change to the UI that is meant is announced, not stored: --expected
names the screens allowed to change, `scenario/name` patterns (fnmatch,
`cadran/*`) against --scenario's name, or `any`; a changed screen it names
counts as announced, not as a failure, and one it names that did not
change is said. CI reads them from the pull request's `Screens:` line.

--update makes <reference-dir> this run's references (main's run, through
visual_refs.py keep): the screenshots of the screens that changed (more
than --max-pixels) replace theirs, the others' files are left untouched,
--add NAME ... and the --screens list add those without a reference yet.
--masked: the images are stored masked already (two sets of references).

Usage: tools/visual_check.py <shots-dir> <reference-dir> [--screens FILE] [--scenario NAME]
                             [--expected LIST] [--update] [--add NAME ...] [--masked]
                             [--fuzz PERCENT] [--max-pixels N]     (needs ImageMagick)
"""
import argparse
import collections
import fnmatch
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile

# The footer clock (bottom left): the host's time, different in every run.
# It is redrawn as the console's time in the smoke test (PLAYSHELF_SIM_NOW
# 1791475200, TZ=UTC), in borealis' footer font, where the label is drawn.
CLOCK_AREA = (30, 655, 220, 50)         # x, y, width, height: the clock only (a dialog starts at x 280)
CLOCK_TEXT = "16:00"
CLOCK_ORIGIN = (55, 692)                # the label's text origin (baseline)
CLOCK_SIZE = "21.5"                     # brls/hints/time fontSize
# With Preferences › Text size's larger text, the footer's clock is 1.15
# times that (HINTS_SCALE, source/kit/ui/cadran_frame.cpp), its baseline a pixel
# lower: a clock at least this tall (its ink, in pixels; 17 to 19 at the
# normal size, drawn here or by the app; 20 to 21 at the larger one) is
# redrawn at that size.
CLOCK_LARGE_SIZE = "24.725"
CLOCK_LARGE_ORIGIN = (55, 693)
CLOCK_LARGE_HEIGHT = 20
CLOCK_FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                          "extern", "borealis", "resources", "font", "switch_font.ttf")
INK = 60   # a pixel this far (sum of the RGB differences) from the background is text
# ImageMagick 7 keeps the version 6 commands (convert, compare, …) as aliases
# of `magick` that warn on every call; version 6 has no `magick`.
IM = ["magick"] if shutil.which("magick") else []


def area_pixels(src):
    """The clock area's pixels, {(x, y): (r, g, b)}, in area coordinates."""
    x, y, w, h = CLOCK_AREA
    out = subprocess.run(IM + ["convert", src, "-crop", f"{w}x{h}+{x}+{y}", "+repage", "-depth", "8", "txt:-"],
                         capture_output=True, text=True, check=True).stdout
    px = {}
    for line in out.splitlines()[1:]:   # "12,3: (235,235,235)  #EBEBEB  srgb(...)"
        m = re.match(r"(\d+),(\d+): \((\d+),(\d+),(\d+)", line)
        if m:
            px[(int(m[1]), int(m[2]))] = (int(m[3]), int(m[4]), int(m[5]))
    return px


MASKED = False   # --masked: the images are stored masked already


def masked(src, dst):
    """`src` with the footer clock showing CLOCK_TEXT: the area filled with
    its background, then the text in the clock's colour (both read from
    `src`, so a footer dimmed under a dialog stays dimmed). No clock there:
    only the background."""
    if MASKED:
        shutil.copyfile(src, dst)
        return
    px = area_pixels(src)
    bg = collections.Counter(px.values()).most_common(1)[0][0]
    distance = lambda c: sum(abs(a - b) for a, b in zip(c, bg))
    ink = [c for c in px.values() if distance(c) > INK]
    x, y, w, h = CLOCK_AREA
    hex_colour = lambda c: "#%02x%02x%02x" % c
    cmd = ["convert", src, "-fill", hex_colour(bg), "-draw", f"rectangle {x},{y} {x + w - 1},{y + h - 1}"]
    if ink:
        # The pixels fully inside a stroke: the text colour.
        far = max(distance(c) for c in ink)
        fg = collections.Counter(c for c in ink if distance(c) >= 0.9 * far).most_common(1)[0][0]
        rows = [y for (_, y), c in px.items() if distance(c) > INK]
        large = max(rows) - min(rows) + 1 >= CLOCK_LARGE_HEIGHT
        size, origin = (CLOCK_LARGE_SIZE, CLOCK_LARGE_ORIGIN) if large else (CLOCK_SIZE, CLOCK_ORIGIN)
        cmd += ["-font", CLOCK_FONT, "-pointsize", size, "-fill", hex_colour(fg),
                "-annotate", f"+{origin[0]}+{origin[1]}", CLOCK_TEXT]
    subprocess.run(cmd + [dst], check=True)


def size(path):
    out = subprocess.run(IM + ["identify", "-format", "%wx%h", path], capture_output=True, text=True, check=True)
    return out.stdout.strip()


def changed_pixels(ref, new, fuzz, diff):
    """Pixels that differ by more than `fuzz` %, with the changes drawn in `diff`."""
    out = subprocess.run(IM + ["compare", "-metric", "AE", "-fuzz", f"{fuzz}%", ref, new, diff],
                         capture_output=True, text=True)
    if out.returncode not in (0, 1):   # 2: compare itself failed
        raise RuntimeError(out.stderr.strip())
    # "1234", or "1234 (0.0188)" in newer ImageMagick versions.
    return int(float(out.stderr.split()[0]))


def compare(ref, new, tmp, args):
    """(changed pixels, why not comparable). Both images are masked into
    `tmp` (ref.png, new.png), the changes drawn in tmp/diff.png. Images of
    different sizes are not compared: every pixel counts as changed."""
    if size(ref) != size(new):
        return float("inf"), f"{size(new)} instead of {size(ref)}"
    ref_m, new_m = os.path.join(tmp, "ref.png"), os.path.join(tmp, "new.png")
    masked(ref, ref_m)
    masked(new, new_m)
    return changed_pixels(ref_m, new_m, args.fuzz, os.path.join(tmp, "diff.png")), None


def read_screens(path):
    """The screen names of a SCREENS file, one a line, in its order."""
    with open(path, encoding="utf-8") as f:
        return [l.strip() for l in f if l.strip() and not l.startswith("#")]


def expected_patterns(text):
    """--expected as patterns: comma- or space-separated; `any` matches every
    screen, `none` or nothing none."""
    items = [p.strip() for p in (text or "").replace(",", " ").split()]
    return [] if items == ["none"] else ["*/*" if p == "any" else p for p in items]


def is_expected(patterns, scenario, name):
    screen = f"{scenario}/{name}" if scenario else name
    return any(fnmatch.fnmatchcase(screen, p) or fnmatch.fnmatchcase(name, p) for p in patterns)


def main():
    global MASKED
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("shots")
    ap.add_argument("reference")
    ap.add_argument("--screens", metavar="FILE", help="the screens compared, one a line (default: every PNG of the references)")
    ap.add_argument("--scenario", default="", help="the scenario's name, what --expected's patterns match before the /")
    ap.add_argument("--expected", default="", metavar="LIST", help="screens allowed to change: scenario/name patterns, or any")
    ap.add_argument("--update", action="store_true", help="make the references this run's screens (the changed ones replaced)")
    ap.add_argument("--add", nargs="*", default=[], metavar="NAME", help="with --update: screens to add")
    ap.add_argument("--masked", action="store_true", help="the images are stored masked already")
    ap.add_argument("--fuzz", type=float, default=15.0)
    ap.add_argument("--max-pixels", type=int, default=100)
    args = ap.parse_args()
    MASKED = args.masked

    listed = read_screens(args.screens) if args.screens else None
    in_refs = sorted(os.path.splitext(os.path.basename(p))[0] for p in glob.glob(os.path.join(args.reference, "*.png")))
    names = listed if listed is not None else in_refs
    if args.update:
        os.makedirs(args.reference, exist_ok=True)
        wanted = sorted(set(names) | set(args.add) | (set(in_refs) if listed is None else set()))
        with tempfile.TemporaryDirectory() as tmp:
            for name in wanted:
                src = os.path.join(args.shots, name + ".png")
                if not os.path.exists(src):
                    sys.exit(f"no {src}")
                ref = os.path.join(args.reference, name + ".png")
                # A screen that did not change keeps its reference as it is,
                # byte for byte: only the changed ones and the new ones are written.
                if os.path.exists(ref) and compare(ref, src, tmp, args)[0] <= args.max_pixels:
                    continue
                # Stored with the footer clock at CLOCK_TEXT, never the host's time.
                print("added" if not os.path.exists(ref) else "updated", name)
                masked(src, ref)
                subprocess.run(IM + ["mogrify", "-strip", ref], check=True)
        # A reference the list no longer names is a screen that left the set.
        for name in in_refs:
            if listed is not None and name not in wanted:
                os.remove(os.path.join(args.reference, name + ".png"))
                print("removed", name)
        print(f"references: {len(wanted)} screen(s) in {args.reference}")
        return 0
    if not names:
        sys.exit(f"no screen to compare: no --screens list and no reference image in {args.reference}")

    patterns = expected_patterns(args.expected)
    failed, announced, new = [], [], []
    diff_dir = os.path.join(args.shots, "visual_diff")
    with tempfile.TemporaryDirectory() as tmp:
        for name in names:
            ref = os.path.join(args.reference, name + ".png")
            new_shot = os.path.join(args.shots, name + ".png")
            if not os.path.exists(new_shot):
                print(f"FAIL {name}: no screenshot {new_shot}")
                failed.append(name)
                continue
            if not os.path.exists(ref):
                print(f"new  {name} (no reference yet)")
                new.append(name)
                continue
            n, why = compare(ref, new_shot, tmp, args)
            if why:
                print(f"FAIL {name}: {why}")
                failed.append(name)
                continue
            if n <= args.max_pixels:
                print(f"ok   {name} ({n} px)")
                continue
            os.makedirs(diff_dir, exist_ok=True)
            subprocess.run(IM + ["convert", os.path.join(tmp, "ref.png"), os.path.join(tmp, "new.png"),
                                 os.path.join(tmp, "diff.png"), "+append", os.path.join(diff_dir, name + ".png")],
                           check=True)
            if is_expected(patterns, args.scenario, name):
                print(f"ok   {name} ({n} px changed, announced)")
                announced.append(name)
                continue
            print(f"FAIL {name}: {n} pixels changed (at most {args.max_pixels})")
            failed.append(name)

    if announced:
        print(f"announced: {', '.join(announced)}")
    if new:
        print(f"new: {', '.join(new)}")
    if args.scenario:
        # The patterns of this scenario (`scenario/…`) that matched no changed screen.
        mine = [p for p in patterns if p != "*/*" and "/" in p and fnmatch.fnmatchcase(args.scenario, p.split("/", 1)[0])]
        unused = [p for p in mine if not any(is_expected([p], args.scenario, n) for n in announced)]
        if unused:
            print("announced but unchanged: " + ", ".join(unused))
    if failed:
        print(f"\nvisual check: {len(failed)} screen(s) changed: {', '.join(failed)}")
        print(f"Side-by-side images (reference | now | changes): {diff_dir}")
        print("If the change is meant, announce it: the pull request's `Screens:` line (tests/visual/README.md)")
        return 1
    print(f"visual check: {len(names) - len(announced) - len(new)} screen(s) unchanged"
          + (f", {len(announced)} changed as announced" if announced else "") + (f", {len(new)} new" if new else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
