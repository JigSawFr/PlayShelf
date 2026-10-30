#!/usr/bin/env python3
"""Writes compat.json, published with every release next to playshelf.nro.

The app reads the one of the latest release (About › Check for updates) to
know whether updating would support the console's firmware. Values come from
the sources, so they can never disagree with the build:
  version            CMakeLists.txt  VERSION_MAJOR / MINOR / ALTER
  fw_tested_max      source/util/firmware.h  APP_FW_TESTED_MAX (verified on a
                     console: a newer release "supports" a firmware only once
                     it was verified there)

Its "schema" is SCHEMA (update::COMPAT_SCHEMA in source/kit/util/update.hpp):
one more when a field changes meaning. Each release publishes it twice:
compat-<SCHEMA>.json, which the app fetches first, and compat.json, which
builds from before it did read and which keeps schema 1 forever (the
schema-1 fields written as they are). A schema 2 would write compat-2.json
and still compat-1.json and compat.json, so installed builds still see it.

Usage: tools/gen_compat.py [repo-root] [output]   (default: . and ./compat.json;
compat-<SCHEMA>.json is written next to the output)
"""
import json
import os
import re
import sys

SCHEMA = 1

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
OUT = sys.argv[2] if len(sys.argv) > 2 else "compat.json"


def fail(msg):
    print("gen_compat:", msg)
    sys.exit(1)


cmake = open(f"{ROOT}/CMakeLists.txt", encoding="utf-8").read()
parts = []
for name in ("MAJOR", "MINOR", "ALTER"):
    m = re.search(rf'set\(VERSION_{name} "(\d+)"\)', cmake)
    if not m:
        fail(f"VERSION_{name} not found in CMakeLists.txt")
    parts.append(m.group(1))

sysinfo = open(f"{ROOT}/source/util/firmware.h", encoding="utf-8").read()


def firmware(macro):
    m = re.search(rf"#define {macro}\s+MAKEHOSVERSION\((\d+),\s*(\d+),\s*(\d+)\)", sysinfo)
    if not m:
        fail(f"{macro} not found in source/util/firmware.h")
    return ".".join(m.groups())


update_hpp = open(f"{ROOT}/source/kit/util/update.hpp", encoding="utf-8").read()
m = re.search(r"constexpr int COMPAT_SCHEMA = (\d+);", update_hpp)
if not m or int(m.group(1)) != SCHEMA:
    fail(f"update::COMPAT_SCHEMA (source/kit/util/update.hpp) is not {SCHEMA}")

compat = {
    "schema": SCHEMA,
    "version": ".".join(parts),
    "fw_tested_max": firmware("APP_FW_TESTED_MAX"),
}
versioned = os.path.join(os.path.dirname(OUT), f"compat-{SCHEMA}.json")
for path in (OUT, versioned):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(compat, f, indent=2)
        f.write("\n")
    print(f"{path}: {json.dumps(compat)}")
