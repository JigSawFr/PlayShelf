#!/usr/bin/env python3
"""Fetches what Atlas publishes, as PlayShelf does, and has PlayShelf's own readers read it.

Usage: tools/atlas_contract.py <harness> [<dir>]

<harness> is the built tools/atlas_contract.cpp; <dir> (default: a temporary folder) is where the
files are kept, laid out like Atlas' data branch. The files come from jsDelivr
(https://cdn.jsdelivr.net/gh/JigSawFr/Atlas@data/), the one place the app asks for them.

A file is *wrong* when:
  - Atlas lists it in index.json and jsDelivr does not serve it (a 403 "Package size exceeded the
    limit of 50 MB" is how a too large data branch shows: the app cannot get that file);
  - or the harness says one of PlayShelf's readers refuses it, or a required file is missing or
    far too small.
A file Atlas does not list and jsDelivr does not have (a 404) is only said: it is not published yet.

Exit status 1 when something is wrong. The report also goes to the job summary of a GitHub run.
"""
import json
import os
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

BASE = "https://cdn.jsdelivr.net/gh/JigSawFr/Atlas@data/"
# What PlayShelf asks of Atlas (action/popular_flow, changes_flow, insights_index).
FILES = [
    "platforms/switch/popular.json", "platforms/switch/games.json", "platforms/switch/similar.json",
    "platforms/switch/editions.json", "platforms/switch/upcoming.json", "platforms/switch/reach.json",
    "platforms/switch/also.json", "changes.json",
]


def get(path, tries=3):
    """(status, body, tries made) of a file of the data branch; status 0 when the network failed.

    A 403 is asked again: jsDelivr has answered "Package size exceeded the limit of 50 MB" for a
    file and served it minutes later. A 404 is an answer."""
    last = (0, b"", 0)
    for attempt in range(1, tries + 1):
        try:
            request = urllib.request.Request(BASE + path, headers={"User-Agent": "playshelf-contract-check"})
            with urllib.request.urlopen(request, timeout=60) as r:
                return r.status, r.read(), attempt
        except urllib.error.HTTPError as e:
            last = (e.code, e.read()[:200], attempt)
            if e.code == 404:
                return last
        except (urllib.error.URLError, TimeoutError, OSError) as e:
            last = (0, str(e).encode(), attempt)
        if attempt < tries:
            time.sleep(5)
    return last


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    harness = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else tempfile.mkdtemp(prefix="atlas-contract-")
    wrong = []
    lines = []

    status, body, _ = get("index.json")
    if status != 200:
        print(f"index.json: {status} {body[:120]!r}")
        return 1
    listed = set(json.loads(body).get("files", {}))
    lines.append(f"index.json lists {len(listed)} files")

    for path in FILES:
        status, body, tries = get(path)
        if status == 200:
            target = os.path.join(out, path)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with open(target, "wb") as f:
                f.write(body)
            lines.append(f"{path}: served ({len(body)} bytes)" + (f", after {tries} tries (jsDelivr refused it first)" if tries > 1 else ""))
        elif path in listed:
            wrong.append(path)
            lines.append(f"{path}: LISTED BY ATLAS BUT NOT SERVED after {tries} tries ({status} {body[:110].decode('utf-8', 'replace')})")
        else:
            lines.append(f"{path}: not published yet ({status})")

    run = subprocess.run([harness, out], capture_output=True, text=True)
    report = "\n".join(lines) + "\n\n" + run.stdout + run.stderr
    if wrong:
        report += f"\nNOT SERVED: {', '.join(wrong)}\n"
    print(report)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write("## Atlas contract\n\n```\n" + report + "```\n")
    return 1 if (run.returncode or wrong) else 0


if __name__ == "__main__":
    sys.exit(main())
