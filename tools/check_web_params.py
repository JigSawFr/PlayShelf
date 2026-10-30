#!/usr/bin/env python3
"""Checks web/params.json, the web simulator's settings panel, and its texts
in web/i18n/ against the desktop simulator and the app (web/README.md).

Errors:
- a PLAYSHELF_SIM_* variable read anywhere in source/ without its entry in
  params.json (its name in lower case without the prefix), or an entry no
  variable matches any more;
- a variable read in source/ that docs/contributing/simulator.md's table
  does not describe, or one it describes that nothing reads;
- `live` on a variable PlayShelf reads once only, into a static;
- an entry of an unknown group or type, a select or a list without options,
  `values` for an option it does not offer, `live` on an entry that is no
  variable, a preset naming an unknown entry, or giving a datetime entry
  neither POSIX seconds nor `@HH:MM`;
- a language of the console (ui::LANGUAGES, Aurore's Cadran's prefs.hpp, but "system") without its
  catalog web/i18n/<language>.json, or not in the page's `lang` entry;
- a text English (web/i18n/en-US.json) lacks: a group, a panel entry's label,
  an option of a select or a list, a preset; or a text in any catalog that
  English does not have (a typo, or a leftover).
Warnings: a text another catalog has not translated yet (the page shows the
English one).

Usage: tools/check_web_params.py [repo-root]
"""
import json
import os
import re
import sys

TYPES = {"bool", "number", "text", "select", "multi", "datetime"}
I18N_PARTS = ("ui", "groups", "params", "presets")


def flat(d, prefix=""):
    """{"params": {"fw": {"label": ...}}} -> {"params/fw/label": ...}"""
    out = {}
    for k, v in d.items():
        if isinstance(v, dict):
            out.update(flat(v, f"{prefix}{k}/"))
        else:
            out[f"{prefix}{k}"] = v
    return out


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    errors, warnings = [], []

    read, once = set(), set()
    for folder, _, files in os.walk(os.path.join(root, "source")):
        for name in files:
            if name.endswith((".c", ".cpp", ".h", ".hpp")):
                with open(os.path.join(folder, name), encoding="utf-8", errors="replace") as f:
                    text = f.read()
                read |= set(re.findall(r"PLAYSHELF_SIM_([A-Z0-9_]+)", text))
                # The shared code (app_id.h) names them APP_ENV "_SIM_<name>".
                read |= set(re.findall(r'APP_ENV\s*"_SIM_([A-Z0-9_]+)"', text))
                # Read once: into a static, or into the console's first state.
                once |= set(re.findall(r"static\s+const\s+[\w:*\s]+=\s*sim_env\w*\(\s*\"PLAYSHELF_SIM_([A-Z0-9_]+)", text))
    # Aurore's Cadran asks for its flags by their short name (ui::hooks'
    # env_flag("DOCKED")), which source/activity/screen_hooks.cpp reads as
    # PLAYSHELF_SIM_<name>.
    cadran = os.path.join(root, "library", "aurore", "library", "lib", "cadran")
    for folder, _, files in os.walk(cadran):
        for name in files:
            if name.endswith((".cpp", ".hpp")):
                with open(os.path.join(folder, name), encoding="utf-8", errors="replace") as f:
                    read |= set(re.findall(r"env_flag\(\s*\"([A-Z0-9_]+)\"", f.read()))

    # The simulator's page: its table of the variables.
    simulator = os.path.join("docs", "contributing", "simulator.md")
    with open(os.path.join(root, simulator), encoding="utf-8") as f:
        documented = set(re.findall(r"PLAYSHELF_SIM_([A-Z0-9_]+)", f.read()))
    if not documented:
        errors.append(f"{simulator}: no PLAYSHELF_SIM_* variable described")
    for v in sorted(read - documented):
        errors.append(f"PLAYSHELF_SIM_{v} is read in source/ but {simulator}'s table does not describe it")
    for v in sorted(documented - read):
        errors.append(f"{simulator} describes PLAYSHELF_SIM_{v}, which nothing in source/ reads any more")

    with open(os.path.join(root, "web", "params.json"), encoding="utf-8") as f:
        manifest = json.load(f)
    groups = [g["id"] for g in manifest["groups"]]

    names, variables = set(), set()
    needed = {f"groups/{g}" for g in groups}   # the texts English must have
    for p in manifest["params"]:
        name = p.get("name", "")
        where = f"params.json: {name or '(no name)'}"
        if not re.fullmatch(r"[a-z0-9_]+", name):
            errors.append(f"{where}: a name in lower case, digits and _")
        if name in names:
            errors.append(f"{where}: twice")
        names.add(name)
        if not p.get("config") and not p.get("page"):
            variables.add(name.upper())
        if p.get("group") not in groups:
            errors.append(f"{where}: no group {p.get('group')!r}")
        if p.get("type") not in TYPES:
            errors.append(f"{where}: type {p.get('type')!r} is not one of {sorted(TYPES)}")
        if p.get("type") in ("select", "multi"):
            options = p.get("options") or []
            if not options or not all(isinstance(o, str) for o in options):
                errors.append(f"{where}: its options, a list of values")
        for o in p.get("values") or {}:
            if o not in (p.get("options") or []):
                errors.append(f"{where}: values for {o!r}, which is not one of its options")
        if p.get("live") and (p.get("config") or p.get("page")):
            errors.append(f"{where}: live, but no variable (config or page)")
        if p.get("live") and name.upper() in once:
            errors.append(f"{where}: live, but PlayShelf reads PLAYSHELF_SIM_{name.upper()} once only")
        if p.get("panel") is False or (p.get("page") and p.get("panel") is not True):
            continue
        needed.add(f"params/{name}/label")
        for o in p.get("options") or []:
            needed.add(f"params/{name}/options/{o}")

    for v in sorted(read - variables):
        errors.append(f"PLAYSHELF_SIM_{v} is read in source/ but has no entry in web/params.json ({v.lower()})")
    for v in sorted(variables - read):
        errors.append(f"web/params.json: {v.lower()} is no PLAYSHELF_SIM_* variable read in source/ any more")

    types = {p.get("name"): p.get("type") for p in manifest["params"]}
    for preset in manifest.get("presets", []):
        needed.add(f"presets/{preset.get('id')}")
        for name, value in preset.get("params", {}).items():
            if name not in names:
                errors.append(f"preset {preset.get('id')}: no entry {name!r}")
            elif types[name] == "datetime" and not re.fullmatch(r"\d+|@([01]\d|2[0-3]):[0-5]\d", str(value)):
                errors.append(f"preset {preset.get('id')}: {name} {value!r} is neither POSIX seconds nor @HH:MM")

    # The console's languages: one catalog each, all in the page's `lang` entry.
    prefs = os.path.join(root, "library", "aurore", "library", "include", "aurore", "cadran", "ui", "prefs.hpp")
    with open(prefs, encoding="utf-8") as f:
        m = re.search(r"LANGUAGES\[\]\s*=\s*\{([^}]*)\}", f.read())
    languages = [l for l in re.findall(r'"([^"]+)"', m.group(1)) if l != "system"] if m else []
    if not languages:
        errors.append(f"{os.path.relpath(prefs, root)}: no LANGUAGES[] found")
    lang = next((p for p in manifest["params"] if p.get("name") == "lang"), {})
    for l in languages:
        if l not in (lang.get("options") or []):
            errors.append(f"params.json: lang does not offer {l}, a language of the console")
        if not os.path.exists(os.path.join(root, "web", "i18n", f"{l}.json")):
            errors.append(f"web/i18n/{l}.json is missing ({l} is a language of the console)")

    catalogs = {}
    folder = os.path.join(root, "web", "i18n")
    for name in sorted(os.listdir(folder)):
        if name.endswith(".json"):
            with open(os.path.join(folder, name), encoding="utf-8") as f:
                catalogs[name[:-5]] = json.load(f)
    english = flat(catalogs.get("en-US", {}))
    for key in sorted(needed - english.keys()):
        errors.append(f"web/i18n/en-US.json: no text for {key}")
    for code, catalog in catalogs.items():
        for part in catalog:
            if part not in I18N_PARTS:
                errors.append(f"web/i18n/{code}.json: {part!r} is not one of {I18N_PARTS}")
        texts = flat(catalog)
        for key in sorted(texts.keys() - english.keys()):
            errors.append(f"web/i18n/{code}.json: {key} is not in English (a typo, or a leftover)")
        if code != "en-US":
            missing = sorted(english.keys() - texts.keys())
            if missing:
                warnings.append(f"web/i18n/{code}.json: {len(missing)} text(s) in English for now, e.g. {missing[0]}")

    for w in warnings:
        print(f"warning: {w}")
    for e in errors:
        print(f"error: {e}")
    if errors:
        print(f"{len(errors)} error(s)")
        return 1
    print(f"{len(variables)} variables, {len(names) - len(variables)} settings, "
          f"{len(manifest.get('presets', []))} presets, {len(catalogs)} languages")
    return 0


if __name__ == "__main__":
    sys.exit(main())
