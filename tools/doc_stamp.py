#!/usr/bin/env python3
"""Stamps a translation of the documentation with the English page it was
translated from (site/README.md): its first line becomes
`<!-- translated from docs/<page>.md, sha256 <12 hex> -->`, the hash of the
English page as it is now. Run it once a translation matches its English page
again; the website says, under a translation whose English page has changed
since, that the English one may be more recent.

Usage: tools/doc_stamp.py docs/<language>/<page>.md ...
"""
import hashlib
import os
import re
import sys

MARK = re.compile(r"<!-- translated from docs/[\w-]+\.md, sha256 [0-9a-f]{12} -->\n")


def stamp(path):
    english = os.path.join("docs", os.path.basename(path))
    with open(english, "rb") as f:
        digest = hashlib.sha256(f.read()).hexdigest()[:12]
    with open(path, encoding="utf-8") as f:
        text = MARK.sub("", f.read(), count=1)
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"<!-- translated from {english}, sha256 {digest} -->\n{text}")
    print(f"{path}: {english} {digest}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    for path in sys.argv[1:]:
        stamp(path)
