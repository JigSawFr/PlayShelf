#!/usr/bin/env python3
"""Refreshes the copies of Atlas' Switch files PlayShelf ships with
(resources/data/: popular.json, games.json), read at start-up until the
newest are fetched (action/popular_flow). Only those two: Home's popular
rows and the names of the games nothing else names have to work offline;
similar.json (megabytes) and the others are only fetched.

Each file is taken from Atlas' data branch through jsDelivr
(https://cdn.jsdelivr.net/gh/JigSawFr/Atlas@data/platforms/switch/<file>),
the same address PlayShelf fetches, and written only when it is Atlas'
schema 1 (library::ATLAS_SCHEMA), for the Switch, with a day and games; a
games.json without titledb's names is refused, as PlayShelf would show
title ids where it has no other name.

Usage: tools/atlas_copies.py [repo] [file…]   (default: both)
"""
import json
import os
import sys
import urllib.request

BASE = "https://cdn.jsdelivr.net/gh/JigSawFr/Atlas@data/platforms/switch/"
FILES = ("popular.json", "games.json")
SCHEMA = 1   # library::ATLAS_SCHEMA


def check(name, data):
    """The reason the file is not one to ship, "" when it is."""
    if not isinstance(data, dict) or data.get("schema") != SCHEMA:
        return f"not Atlas' schema {SCHEMA}"
    if data.get("platform", "switch") != "switch":
        return "not the Switch's"
    if not isinstance(data.get("generated"), str) or not isinstance(data.get("games"), dict) or not data["games"]:
        return "no day or no games"
    if name == "games.json":
        named = sum(1 for g in data["games"].values() if isinstance(g, dict) and g.get("name"))
        if named * 2 < len(data["games"]):
            return f"only {named} of {len(data['games'])} games named"
    return ""


def main():
    args = sys.argv[1:]
    repo = args.pop(0) if args and os.path.isdir(args[0]) else "."
    names = args or list(FILES)
    failed = 0
    for name in names:
        if name not in FILES:
            print(f"{name}: not one of {', '.join(FILES)}", file=sys.stderr)
            failed += 1
            continue
        with urllib.request.urlopen(BASE + name, timeout=60) as r:
            raw = r.read()
        why = check(name, json.loads(raw))
        if why:
            print(f"{name}: {why}, left as it is", file=sys.stderr)
            failed += 1
            continue
        path = os.path.join(repo, "resources", "data", name)
        with open(path, "wb") as f:
            f.write(raw)
        data = json.loads(raw)
        print(f"{path}: {data['generated']}, {len(data['games'])} games, {len(raw)} bytes")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
