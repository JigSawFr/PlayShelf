#!/usr/bin/env python3
"""The visual check's reference screens: main's own, never files kept by
hand.

Git keeps only tests/visual/<scenario>/SCREENS, the names of the stable
screens compared (tools/visual_check.py). The references are the screens
of the last run of main: each smoke shard keeps its scenarios' screens,
masked, in .visual-refs/<scenario>/ (the Actions cache from one run to the
next, and a `-refs-<shard>` artifact of the run); a pull request restores
main's and compares with them, and announces the screens it changes on
purpose on its `Screens:` line. Each release attaches them as
playshelf-screens.zip, which the release page compares with the previous
release's, and which a checkout without a cache fetches.

  keep <shots-dir> <scenario>   main's step: the scenario's SCREENS from
                                <shots-dir> into .visual-refs/<scenario>/
                                (the changed ones replaced, the new ones
                                added, those the list left removed)
  fetch [scenario ...]          .visual-refs/ for the scenarios without
                                one: the latest release's zip (--release
                                TAG for another, --zip FILE for a local
                                one), or a run's refs artifacts with
                                --run ID (gh)
  zip <refs-dir> <out.zip>      the references as a release attaches them
  diff <before-dir> <after-dir> what changed between two sets (two
                                releases' zips unpacked): --json the
                                lists, --sheet a before/after image

PLAYSHELF_VISUAL_REFS names another folder than .visual-refs/.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HERE = os.path.dirname(os.path.abspath(__file__))
SCREENS_DIR = os.path.join(ROOT, "tests", "visual")
REFS = os.environ.get("PLAYSHELF_VISUAL_REFS") or os.path.join(ROOT, ".visual-refs")
REPO = os.environ.get("PLAYSHELF_REPO") or os.environ.get("GITHUB_REPOSITORY") or "JigSawFr/PlayShelf"
ASSET = "playshelf-screens.zip"
SHEET = "playshelf-screens-changed.png"
IM = ["magick"] if shutil.which("magick") else []


def scenarios(names):
    """The scenarios asked for that have a SCREENS list (every one with one
    by default); one without any (a scenario with no stable screen: nothing
    to compare) is said and left out."""
    if names:
        kept = []
        for n in names:
            if os.path.exists(os.path.join(SCREENS_DIR, n, "SCREENS")):
                kept.append(n)
            else:
                print(f"{n}: no tests/visual/{n}/SCREENS, no stable screen to compare")
        return kept
    return sorted(d for d in os.listdir(SCREENS_DIR) if os.path.exists(os.path.join(SCREENS_DIR, d, "SCREENS")))


def visual_check(*args):
    """tools/visual_check.py: its output and its status."""
    p = subprocess.run([sys.executable, os.path.join(HERE, "visual_check.py"), *args], capture_output=True, text=True)
    return p.stdout + p.stderr, p.returncode


def cmd_keep(a):
    screens = os.path.join(SCREENS_DIR, a.scenario, "SCREENS")
    if not os.path.exists(screens):
        sys.exit(f"no {screens}")
    out, rc = visual_check(a.shots, os.path.join(a.refs, a.scenario), "--update", "--screens", screens)
    sys.stdout.write(out)
    return rc


def download(url, dst):
    """`url` into `dst`; False when it cannot be had (no such release or
    asset, or no network: the references are a comparison, not a need)."""
    try:
        with urllib.request.urlopen(url, timeout=60) as r, open(dst, "wb") as f:
            shutil.copyfileobj(r, f)
    except urllib.error.HTTPError as e:
        if e.code != 404:
            print(f"{url}: HTTP {e.code}")
        return False
    except (urllib.error.URLError, OSError, TimeoutError) as e:
        print(f"{url}: {e}")
        return False
    return True


def unzip_refs(zip_path, into, only=None):
    """<scenario>/<name>.png of the zip into `into`, the scenarios of `only`
    (every one by default); returns the scenarios unpacked."""
    done = set()
    with zipfile.ZipFile(zip_path) as z:
        for info in z.infolist():
            parts = info.filename.split("/")
            if len(parts) != 2 or not parts[1].endswith(".png") or not parts[0] or ".." in parts:
                continue
            if only is not None and parts[0] not in only:
                continue
            os.makedirs(os.path.join(into, parts[0]), exist_ok=True)
            with z.open(info) as src, open(os.path.join(into, parts[0], parts[1]), "wb") as dst:
                shutil.copyfileobj(src, dst)
            done.add(parts[0])
    return done


def cmd_fetch(a):
    wanted = [s for s in scenarios(a.scenarios) if a.force or not os.path.isdir(os.path.join(a.refs, s))]
    if not wanted:
        print("every scenario has its references already")
        return 0
    os.makedirs(a.refs, exist_ok=True)
    if a.run:
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run(["gh", "run", "download", a.run, "-R", REPO, "-p", "*-refs-*", "-D", tmp], check=True)
            got = set()
            for folder in os.listdir(tmp):
                for sc in os.listdir(os.path.join(tmp, folder)):
                    if sc in wanted and os.path.isdir(os.path.join(tmp, folder, sc)):
                        shutil.copytree(os.path.join(tmp, folder, sc), os.path.join(a.refs, sc), dirs_exist_ok=True)
                        got.add(sc)
        source = f"run {a.run}"
    else:
        if a.zip:
            zip_path, source = a.zip, a.zip
        else:
            tag = a.release or "latest"
            url = (f"https://github.com/{REPO}/releases/latest/download/{ASSET}" if tag == "latest"
                   else f"https://github.com/{REPO}/releases/download/{tag}/{ASSET}")
            zip_path = os.path.join(a.refs, ASSET)
            if not download(url, zip_path):
                print(f"no {ASSET} from the {tag} release of {REPO}: no references yet for {', '.join(wanted)}")
                return 0
            source = url
        got = unzip_refs(zip_path, a.refs, set(wanted))
        if not a.zip:
            os.remove(zip_path)
    for sc in wanted:
        print(f"{sc}: {'fetched' if sc in got else 'not in'} {source}")
    return 0


def cmd_zip(a):
    """Every <scenario>/<name>.png of the folder, sorted, one date (the zip
    depends on the files alone)."""
    files = sorted(os.path.join(sc, f) for sc in os.listdir(a.refs) if os.path.isdir(os.path.join(a.refs, sc))
                   for f in os.listdir(os.path.join(a.refs, sc)) if f.endswith(".png"))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with zipfile.ZipFile(a.out, "w", zipfile.ZIP_DEFLATED) as z:
        for rel in files:
            info = zipfile.ZipInfo(rel.replace(os.sep, "/"), date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(os.path.join(a.refs, rel), "rb") as f:
                z.writestr(info, f.read())
    print(f"{a.out}: {len(files)} screen(s)")
    return 0


def sheet_rows(before, after, screens, tmp):
    """One row an image: the screen's name above, before | after (a blank
    where there is none), each 480 px wide."""
    rows = []
    for i, (sc, name, state) in enumerate(screens):
        b = os.path.join(before, sc, name + ".png")
        n = os.path.join(after, sc, name + ".png")
        row = os.path.join(tmp, f"row{i}.png")
        blank = ["-size", "480x270", "xc:#2a2f3a"]
        left = [b, "-resize", "480x"] if os.path.exists(b) else blank
        right = [n, "-resize", "480x"] if os.path.exists(n) else blank
        label = f"{sc}/{name}" + {"changed": "", "new": "  (new)", "gone": "  (gone)"}[state]
        subprocess.run(IM + ["convert", *left, *right, "+append", "-background", "#1b1f2a", "-gravity", "NorthWest",
                             "-splice", "0x30", "-fill", "white", "-pointsize", "18", "-annotate", "+8+6", label,
                             "-bordercolor", "#1b1f2a", "-border", "0x6", row], check=True)
        rows.append(row)
    return rows


def cmd_diff(a):
    before_sc = {d for d in os.listdir(a.before) if os.path.isdir(os.path.join(a.before, d))} if os.path.isdir(a.before) else set()
    after_sc = {d for d in os.listdir(a.after) if os.path.isdir(os.path.join(a.after, d))}
    changed, new, gone = [], [], []
    for sc in sorted(after_sc | before_sc):
        b, n = os.path.join(a.before, sc), os.path.join(a.after, sc)
        names_b = {f[:-4] for f in os.listdir(b) if f.endswith(".png")} if os.path.isdir(b) else set()
        names_n = {f[:-4] for f in os.listdir(n) if f.endswith(".png")} if os.path.isdir(n) else set()
        new += [(sc, x, "new") for x in sorted(names_n - names_b)]
        gone += [(sc, x, "gone") for x in sorted(names_b - names_n)]
        if names_b & names_n:
            # The screens both have, compared masked as they are stored.
            out, rc = visual_check(n, b, "--masked")
            changed += [(sc, m.group(1), "changed") for m in re.finditer(r"^FAIL (\S+): \d+ pixels changed", out, re.M)]
    result = {"changed": [f"{sc}/{x}" for sc, x, _ in changed], "new": [f"{sc}/{x}" for sc, x, _ in new],
              "gone": [f"{sc}/{x}" for sc, x, _ in gone], "sheet": ""}
    shown = changed + new + gone
    if a.sheet and shown:
        with tempfile.TemporaryDirectory() as tmp:
            rows = sheet_rows(a.before, a.after, shown[: a.max_rows], tmp)
            subprocess.run(IM + ["convert", *rows, "-append", a.sheet], check=True)
        result["sheet"] = os.path.basename(a.sheet)
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=1)
    print(f"{len(changed)} changed, {len(new)} new, {len(gone)} gone" + (f"; sheet: {a.sheet}" if result["sheet"] else ""))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--refs", default=REFS, help="the references' folder (.visual-refs)")
    sub = ap.add_subparsers(dest="command", required=True)
    p = sub.add_parser("keep", help="main's step: a scenario's screens become its references")
    p.add_argument("shots"); p.add_argument("scenario")
    p.set_defaults(func=cmd_keep)
    p = sub.add_parser("fetch", help="the references of the scenarios without one")
    p.add_argument("scenarios", nargs="*", metavar="scenario")
    p.add_argument("--release", help="a release's tag (default: the latest)")
    p.add_argument("--zip", help="a playshelf-screens.zip at hand")
    p.add_argument("--run", help="a workflow run's id: its refs artifacts, with gh")
    p.add_argument("--force", action="store_true", help="also the scenarios that have references")
    p.set_defaults(func=cmd_fetch)
    p = sub.add_parser("zip", help="the references as a release attaches them")
    p.add_argument("refs"); p.add_argument("out")
    p.set_defaults(func=cmd_zip)
    p = sub.add_parser("diff", help="what changed between two sets of references")
    p.add_argument("before"); p.add_argument("after")
    p.add_argument("--json", help="the lists, as JSON")
    p.add_argument("--sheet", help="a before/after image of the screens that differ")
    p.add_argument("--max-rows", type=int, default=60)
    p.set_defaults(func=cmd_diff)
    a = ap.parse_args()
    return a.func(a)


if __name__ == "__main__":
    sys.exit(main())
