#!/usr/bin/env python3
"""Static checks on the app resources (run in CI and by `make check`).

- every i18n JSON file parses, and every language (resources/i18n/<code>/
  playshelf.json) defines exactly the keys of en-US, with as many "{}";
- config::LANGUAGES (source/util/config.hpp) lists exactly those languages,
  and each has a name under playshelf/tools/languages/;
- every "playshelf/..." key referenced from C++ or XML exists in en-US,
  but PlayShelf's theme colours and image slots (THEME_KEYS:
  "playshelf/dial/...", "playshelf/cover/...", keys of the theme, not of the
  catalogs), which docs/themes.md lists each;
- a key built at runtime ("playshelf/days/{}", "playshelf/error/" + key)
  is under a prefix DYNAMIC_KEYS declares, with the values the code
  can put after it (read from the code's own lists where there is one): each
  has its key, and a key under the prefix that is none of them is unused;
- every en-US key is referenced (exactly, or as such a value): no dead
  strings for translators to keep up to date;
- every XML layout is well formed (borealis' "brls:" prefix is not declared,
  so a non-namespace-aware parser is used);
- brls:Label never carries padding attributes (borealis throws at runtime);
- every BRLS_BIND(type, name, "id") names an id of the XML its class
  inflates (CONTENT_FROM_XML_RES, TabBase("xml/..."), inflateFromXMLRes,
  createFromXMLResource), or
  of a custom view that XML holds (<LibraryTab/>): a missing one is a null
  view at runtime, the first use of which crashes. Every class of a header
  and of its .cpp counts.
- every line id a Cadran screen names (source/**/cadran_*.cpp: getView,
  park, apply, tiles, press...) is an id of a layout or of a view the code
  makes (setId): a renamed line would leave a chip that does nothing;
- every sidebar tab name (playshelf/tabs/) fits on one line: at most
  TAB_WIDTH columns, a wide (CJK) character counting as two;
- every key ending in "_one" (the text for exactly one) has the key without
  it next to it (the text for any other count), in every language;
- every page of the documentation the app links to (app::doc_url("page",
  "anchor"), a QR code on the console) is docs/<page>.md, with a heading
  whose anchor is that one: an app already installed keeps the link, so a
  page renamed or a heading reworded breaks it; and the app links to the
  documentation only through doc_url, never a link of its own to docs/;
- no font of the repository or of Aurore's resources (what the builds ship)
  names a console maker or its type foundry (Nintendo, Morisawa): their
  system fonts are not free, the console has its own;
- no file of the repository (Aurore's included) is a console's key file
  (prod.keys, title.keys…) or a game package or one of its parts (.nsp, .xci,
  .nca, .tik…): PlayShelf never carries what belongs to the console or a game.

It also warns, without failing:
- about sentence-like values a catalog still has in English (English is a
  fine placeholder for a new string);
- about a value that marks a plural in brackets ("{} saved(s)", "Tag(e)"):
  the code picks the text for the count (a "_one" key for 1) instead;
- about a Cadran value of pt-BR in European Portuguese ("ecrã", "ficheiro"):
  Brazil says "tela", "arquivo";
- about an XML layout of resources/xml no class inflates, and an image of
  resources/img nothing names (the emoji of img/emoji/ aside: a palette for
  the release notes, kept whole on purpose);
- about an emoji a heading or a bullet of CHANGELOG.md or
  .github/release-notes/ opens on that has no img/emoji/<hex>.png (it shows
  no icon on the console);
- about a key docs/config.md documents that source/util/config.cpp neither
  reads nor writes, and one config.cpp reads or writes that the page leaves
  out;
- about a PLAYSHELF_SIM_* variable the code reads that
  docs/contributing/simulator.md does not document, or one it documents that
  nothing reads, and about a tools/desktop_smoke.py scenario the list of
  docs/contributing/tests.md leaves out, or one it lists that the script does
  not have;
- about a translation of a page the app links to whose sections are not the
  English page's (the website then opens the page, not the section).
"""
import glob
import json
import os
import re
import sys
import unicodedata
import xml.parsers.expat

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
errors = []
warnings = []
# Names that read the same in every language.
SAME_IN_EVERY_LANGUAGE = {"about/made_in_france", "tools/update_via_values/appstore", "update/appstore",
                          "tools/storage_emummc", "tools/storage_sysmmc", "init_error/firmware"}


def flatten(d, prefix=""):
    out = {}
    for k, v in d.items():
        if isinstance(v, dict):
            out.update(flatten(v, prefix + k + "/"))
        else:
            out[prefix + k] = v
    return out


BASE = "en-US"
catalogs = {}
for path in sorted(glob.glob(f"{ROOT}/resources/i18n/*/playshelf.json")):
    lang = path.split("/")[-2]
    try:
        catalogs[lang] = flatten(json.load(open(path, encoding="utf-8")))
    except Exception as e:  # noqa: BLE001
        errors.append(f"{path}: {e}")
for path in glob.glob(f"{ROOT}/resources/i18n/*/*.json"):
    if not path.endswith("/playshelf.json"):
        try:
            json.load(open(path, encoding="utf-8"))
        except Exception as e:  # noqa: BLE001
            errors.append(f"{path}: {e}")
if BASE not in catalogs:
    errors.append(f"resources/i18n/{BASE}/playshelf.json is missing")

base = catalogs.get(BASE, {})
for lang, cat in sorted(catalogs.items()):
    if lang == BASE:
        continue
    for k in sorted(set(base) - set(cat)):
        errors.append(f"{lang} is missing {k}")
    for k in sorted(set(cat) - set(base)):
        errors.append(f"{lang} has extra key {k}")
    # Still in English: a value identical to en-US with three or more words is
    # most likely a placeholder nobody translated (names and brands aside).
    for k in sorted(set(base) & set(cat)):
        v = str(cat[k])
        if v == str(base[k]) and k not in SAME_IN_EVERY_LANGUAGE and not k.startswith("tools/languages/") \
                and len(re.findall(r"[A-Za-z]{2,}", v)) >= 3:
            warnings.append(f"{lang}: {k} is still in English")
    # The same number of "{}" placeholders as en-US.
    for k in sorted(set(base) & set(cat)):
        if str(base[k]).count("{}") != str(cat[k]).count("{}"):
            errors.append(f"{lang}: placeholder count differs for {k}")

# A text for exactly one ("files_one") goes with the text for any other count
# ("files"): the code reads the second one for every count but 1.
for lang, cat in sorted(catalogs.items()):
    for k in sorted(cat):
        if k.endswith("_one") and k[:-len("_one")] not in cat:
            errors.append(f"{lang}: {k} has no {k[:-len('_one')]} (the text for any other count)")

# A plural marked in brackets ("enregistré(s)", "Tag(e)", "salvo(s)") reads
# badly: the code gives the count, so the catalog can have one text for 1
# ("_one") and one for more.
PLURAL_MARK = re.compile(r"\w\((?:s|e|es|en|n|x|ões)\)")
for lang, cat in sorted(catalogs.items()):
    for k, v in sorted(cat.items()):
        m = PLURAL_MARK.search(str(v))
        if m:
            warnings.append(f"{lang}: {k} marks a plural in brackets (\"{m.group(0)[1:]}\"): a \"_one\" key for 1 reads better")

# pt-BR is Brazilian Portuguese: the Cadran screens, translated from pt,
# must not keep the words only Portugal uses (Brazil says "tela",
# "arquivo", "celular", "por enquanto", "Controle").
EUROPEAN_PORTUGUESE = ["ecrã", "ficheiro", "telemóvel", "por agora", "controlo"]
for k, v in sorted(catalogs.get("pt-BR", {}).items()):
    if k.startswith("cadran/"):
        for word in EUROPEAN_PORTUGUESE:
            if re.search(r"\b" + re.escape(word) + r"\b", str(v), re.I):
                warnings.append(f"pt-BR: {k} says \"{word}\", which is European Portuguese (pt), not Brazilian")

# The sidebar's tab names stay on one line in every language.
TAB_WIDTH = 16
for lang, cat in sorted(catalogs.items()):
    for k, v in cat.items():
        if k.startswith("tabs/"):
            width = sum(2 if unicodedata.east_asian_width(c) in "WF" else 1 for c in v)
            if width > TAB_WIDTH:
                errors.append(f"{lang}: tab name {k} \"{v}\" is {width} columns wide, more than {TAB_WIDTH}")

# The language picker's list (Aurore's ui::LANGUAGES, which config::LANGUAGES
# names) and the catalogs agree; the languages' names are Aurore's
# (cadran/tools/languages/).
PREFS_HPP = "library/aurore/library/include/borealis/cadran/ui/prefs.hpp"
prefs_hpp = open(f"{ROOT}/{PREFS_HPP}", encoding="utf-8").read()
m = re.search(r"LANGUAGES\[\]\s*=\s*\{([^}]*)\}", prefs_hpp)
if not m:
    errors.append(f"{PREFS_HPP}: ui::LANGUAGES not found")
else:
    listed = re.findall(r'"([^"]+)"', m.group(1))
    if not listed or listed[0] != "system":
        errors.append('ui::LANGUAGES must start with "system"')
    for lang in sorted(set(listed) - {"system"} - set(catalogs)):
        errors.append(f"ui::LANGUAGES lists {lang}, which has no resources/i18n/{lang}/playshelf.json")
    for lang in sorted(set(catalogs) - set(listed)):
        errors.append(f"resources/i18n/{lang} is not in ui::LANGUAGES (the language picker)")

en_keys = set(base)
prefixes = {k.rsplit("/", 1)[0] + "/" for k in en_keys}


used_exact = set()
used_prefixes = set()


def check_key(key, where):
    if "{" in key or key.endswith("/"):
        prefix = key.split("{")[0]
        used_prefixes.add(prefix.replace("playshelf/", "", 1))
        if not any(p.startswith(prefix.replace("playshelf/", "", 1)) for p in prefixes):
            errors.append(f"{where}: no key under prefix {key}")
        return
    used_exact.add(key.replace("playshelf/", "", 1))
    if key.replace("playshelf/", "", 1) not in en_keys:
        errors.append(f"{where}: unknown i18n key {key}")


# PlayShelf's own theme colours (View::setThemeColor, theme files) and image
# slots (Theme::addImageSlot, `image.<key>` in theme files): the prefixes
# they are under, documented for theme makers in docs/themes.md.
THEME_KEYS = ("playshelf/dial/", "playshelf/cover/")
theme_doc = open(f"{ROOT}/docs/themes.md", encoding="utf-8").read()

for path in glob.glob(f"{ROOT}/source/**/*.[ch]pp", recursive=True) + glob.glob(f"{ROOT}/source/**/*.[ch]", recursive=True):
    text = open(path, encoding="utf-8").read()
    for m in re.finditer(r'"(playshelf/[A-Za-z0-9_/{}.-]*)"', text):
        if m.group(1).startswith(THEME_KEYS):
            if f"`{m.group(1)}`" not in theme_doc:
                errors.append(f"{path}: the theme key {m.group(1)} is not in docs/themes.md")
            continue
        check_key(m.group(1), path)

for path in sorted(glob.glob(f"{ROOT}/resources/xml/**/*.xml", recursive=True)):
    data = open(path, "rb").read()
    parser = xml.parsers.expat.ParserCreate()

    def start(name, attrs, path=path):
        if name == "brls:Label" and any(a.startswith("padding") for a in attrs):
            errors.append(f"{path}: brls:Label does not support padding attributes")
        for value in attrs.values():
            if value.startswith("@i18n/playshelf/"):
                check_key(value[len("@i18n/"):], path)

    parser.StartElementHandler = start
    try:
        parser.Parse(data, True)
    except xml.parsers.expat.ExpatError as e:
        errors.append(f"{path}: {e}")


def read(path):
    try:
        return open(path, encoding="utf-8").read()
    except OSError:
        return ""


def strings_of(text):
    return re.findall(r'"([^"\\]*)"', text)


def array_of(path, name):
    """The string literals of the array `name` (`NAME[] = { ... }` or a
    vector `NAME = { ... }`) in `path`."""
    m = re.search(r"\b" + re.escape(name) + r"\s*(?:\[\w*\])?\s*=\s*\{(.*?)\};", read(f"{ROOT}/{path}"), re.S)
    return [v for v in strings_of(m.group(1)) if v] if m else []


def function_of(path, name):
    """The body of the function `name` in `path` (up to the next closing brace
    at the start of a line)."""
    m = re.search(r"\b" + re.escape(name) + r"\([^)]*\)\s*\{(.*?)^\}", read(f"{ROOT}/{path}"), re.S | re.M)
    return m.group(1) if m else ""


def returned_by(path, name):
    """The string literals `name` returns ("" aside)."""
    return [v for v in re.findall(r'return\s+"([^"]*)"', function_of(path, name)) if v]


def enum_count(path, name):
    """The enumerators of `enum class name` before its `Count`."""
    m = re.search(r"enum\s+class\s+" + re.escape(name) + r"\s*\{(.*?)\}", read(f"{ROOT}/{path}"), re.S)
    if not m:
        return 0
    body = re.sub(r"//[^\n]*", "", m.group(1))
    names = [n.strip().split("=")[0].strip() for n in body.split(",") if n.strip()]
    return names.index("Count") if "Count" in names else len(names)


def numbers(n):
    return [str(i) for i in range(n)]


# Every prefix a key is built under at runtime, with the values the code can
# put after it: from the code's own list where there is one, else written
# out here, next to where the code counts them. A new prefix, or a value the
# list gains, needs its keys; a key no value names is reported unused.
DYNAMIC_KEYS = {
    # The code's lists (the days, months, languages, themes and text sizes
    # are Aurore's Cadran's, in its own cadran.json).
    "error/": strings_of(re.sub(r"//[^\n]*", "", function_of("source/kit/action/error_hint_logic.cpp", "hint_key"))),
    "tabs/": array_of("source/util/config.hpp", "START_TABS"),
    "prefs/effects_values/": array_of(PREFS_HPP, "EFFECTS"),
    "tools/update_via_values/": array_of("source/kit/action/update_flow.cpp", "METHODS"),
    "library/sort_values/": array_of("source/tab/catalogue_tab.cpp", "ORDERS"),
    "genres/": array_of("source/action/nlib_flow.hpp", "GENRES"),
    "installed/order_values/": array_of("source/tab/library_tab.cpp", "ORDERS"),
    "languages/": array_of("source/titledb/nlib.hpp", "NLIB_LANGUAGES"),
    "library/connection_values/": array_of("source/activity/server_activity.cpp", "CONNECTIONS"),
    "library/format_values/": array_of("source/activity/server_activity.cpp", "FORMATS"),
    # A failed download's cause: every cause's name but those the Downloads
    # tab words otherwise (its own error, the unsigned content's line).
    "downloads/cause/": [v for v in returned_by("source/download/recovery.cpp", "cause_name") if v not in ("other", "unsigned")],
    "downloads/help/": [v for v in returned_by("source/download/recovery.cpp", "cause_name") if v not in ("other", "unsigned")],
}
for prefix, values in sorted(DYNAMIC_KEYS.items()):
    values = [v for v in values if v]
    DYNAMIC_KEYS[prefix] = set(values)
    if not values:
        errors.append(f"tools/check_resources.py: no value found for keys under playshelf/{prefix} (DYNAMIC_KEYS)")
    for v in sorted(set(values)):
        if prefix + v not in en_keys:
            errors.append(f"the code builds playshelf/{prefix}{v}, which en-US does not have")
for prefix in sorted(used_prefixes - set(DYNAMIC_KEYS)):
    errors.append(f"a key is built at runtime under playshelf/{prefix}: declare its values in "
                  f"tools/check_resources.py (DYNAMIC_KEYS)")


def dynamic(k):
    return any(k.startswith(p) and k[len(p):] in vs for p, vs in DYNAMIC_KEYS.items())


for k in sorted(en_keys):
    if k not in used_exact and not dynamic(k):
        errors.append(f"unused i18n key playshelf/{k} (remove it from every language)")

code_text = {p: read(p) for p in glob.glob(f"{ROOT}/source/**/*.[ch]pp", recursive=True)}


# BRLS_BIND ids against the XML each class inflates. A class's XML is named in
# its header (an activity's CONTENT_FROM_XML_RES("activity/x.xml"), under
# resources/xml/) or in its .cpp (a tab's TabBase("xml/tab/x.xml"), a view's
# inflateFromXMLRes("xml/view/x.xml"), under resources/; a Cadran tab's top,
# createFromXMLResource("view/x.xml"), under resources/xml/). Each one, and each
# BRLS_BIND, belongs to the class around it: the last "class X" before it, or
# the last "X::f(" a definition at the start of a line opens, whichever is
# nearer; in the header or in its .cpp.
xml_dir = f"{ROOT}/resources/xml"
code_files = glob.glob(f"{ROOT}/source/**/*.[ch]pp", recursive=True)
class_xml = {}      # class name -> the XML paths it inflates, relative to resources/xml
class_binds = {}    # class name -> (file, [ids])
loaded_xml = set()
OWNER = re.compile(r"^\s*class\s+(\w+)\b[^;]*$|^(?:[A-Za-z_][^\n;{}]*?[\s*&])?([A-Za-z_]\w*)::~?\w+\s*\(", re.M)


def owner_at(text, pos):
    name = None
    for m in OWNER.finditer(text, 0, pos):
        name = m.group(1) or m.group(2)
    return name


pairs = sorted({p[:-len(".hpp")] if p.endswith(".hpp") else p[:-len(".cpp")]
                for p in glob.glob(f"{ROOT}/source/**/*.[ch]pp", recursive=True)})
for stem in pairs:
    for path in (stem + ".hpp", stem + ".cpp"):
        text = read(path)
        for m in re.finditer(r'(?:CONTENT_FROM_XML_RES|createFromXMLResource)\(\s*"([^"]+)"|(?:TabBase|inflateFromXMLRes)\(\s*"xml/([^"]+)"', text):
            name = owner_at(text, m.start())
            if name:
                class_xml.setdefault(name, []).append(m.group(1) or m.group(2))
        for m in re.finditer(r'BRLS_BIND\([^;]*?,\s*\w+\s*,\s*"([^"]+)"\s*\)', text):
            name = owner_at(text, m.start())
            if name:
                class_binds.setdefault(name, (path, []))[1].append(m.group(1))
for path in code_files:
    text = read(path)
    loaded_xml.update(re.findall(r'CONTENT_FROM_XML_RES\(\s*"([^"]+)"', text))
    loaded_xml.update(re.findall(r'"xml/([^"]+\.xml)"', text))   # TabBase, inflateFromXMLRes
    loaded_xml.update(re.findall(r'createFromXMLResource\(\s*"([^"]+\.xml)"', text))

xml_ids = {}    # XML path -> (ids, element names)
for path in glob.glob(f"{xml_dir}/**/*.xml", recursive=True):
    ids, tags = set(), set()
    parser = xml.parsers.expat.ParserCreate()

    def start(name, attrs, ids=ids, tags=tags):
        tags.add(name)
        if "id" in attrs:
            ids.add(attrs["id"])

    parser.StartElementHandler = start
    try:
        parser.Parse(open(path, "rb").read(), True)
    except xml.parsers.expat.ExpatError:
        pass   # reported above
    xml_ids[path[len(xml_dir) + 1:]] = (ids, tags)


def ids_of(res, seen):
    """The ids of `res`, with those of the custom views it holds."""
    if res in seen or res not in xml_ids:
        return set()
    seen.add(res)
    ids, tags = xml_ids[res]
    out = set(ids)
    for tag in tags:
        for inflated in class_xml.get(tag, []):
            out |= ids_of(inflated, seen)
    return out


for name, (path, binds) in sorted(class_binds.items()):
    inflated = class_xml.get(name)
    if not inflated:
        warnings.append(f"{path}: {name} binds views but no XML it inflates was found")
        continue
    missing = [res for res in inflated if res not in xml_ids]
    for res in missing:
        errors.append(f"{path}: {name} inflates {res}, which is not in resources/xml")
    if missing:
        continue
    ids = set()
    for res in inflated:   # a screen may put one layout inside another
        ids |= ids_of(res, set())
    for i in binds:
        if i not in ids:
            errors.append(f"{path}: BRLS_BIND id \"{i}\" is not in resources/xml/{' or '.join(inflated)}")

# The lines a screen finds by id (getView, apply, tiles, banners, open_tab...):
# each one an id some layout or the code still gives a view. A renamed line
# is otherwise a chip or a button that does nothing. A literal counts as a
# line's id when it starts like the layouts' ids do ("dash_", "cn_"...).
all_ids = set()
for path in glob.glob(f"{ROOT}/resources/xml/**/*.xml", recursive=True):
    all_ids |= set(re.findall(r'\bid="([^"]+)"', read(path)))
id_prefixes = {i.split("_")[0] for i in all_ids if "_" in i and "/" not in i}
for text in code_text.values():
    all_ids |= set(re.findall(r'setId\("([^"]+)"\)', text)) | set(re.findall(r'\bid="([^"]+)"', text))
for path in sorted(set(glob.glob(f"{ROOT}/source/tab/*.cpp")) | set(glob.glob(f"{ROOT}/source/**/cadran_*.cpp", recursive=True))):
    for i in sorted(set(re.findall(r'"([a-z]+_[a-z0-9_]+)"', read(path)))):
        if i.split("_")[0] in id_prefixes and i not in all_ids:
            errors.append(f"{path}: \"{i}\" names no view (no layout's id, no setId): a renamed line?")

# XML layouts nothing loads.
for res in sorted(set(xml_ids) - loaded_xml):
    warnings.append(f"resources/xml/{res} is never loaded (no CONTENT_FROM_XML_RES, TabBase, inflateFromXMLRes or createFromXMLResource)")

# Images nothing names ("img/x.png" in the code, "@res/img/x.png" in a layout).
# The emoji are a palette for the release notes (img/emoji/README.md): an
# unused one is fine; an emoji the notes open on without its file is not.
named = "\n".join(read(p) for p in code_files + glob.glob(f"{xml_dir}/**/*.xml", recursive=True))
for path in sorted(glob.glob(f"{ROOT}/resources/img/**/*", recursive=True)):
    rel = path[len(f"{ROOT}/resources/"):]
    if os.path.isdir(path) or rel.startswith("img/emoji/"):
        continue
    if rel not in named:
        warnings.append(f"resources/{rel} is never used (no \"{rel}\" in the code or a layout)")


def is_emoji(cp):
    """The pictographs source/kit/util/changelog.cpp draws as images."""
    return cp >= 0x1F000 or 0x2600 <= cp <= 0x27BF or 0x2B00 <= cp <= 0x2BFF or \
        0x231A <= cp <= 0x231B or 0x23E9 <= cp <= 0x23FA


notes = [f"{ROOT}/CHANGELOG.md"] + sorted(p for p in glob.glob(f"{ROOT}/.github/release-notes/*.md")
                                         if not p.endswith("/README.md"))   # how to write them
missing_emoji = {}
for path in notes:
    for line in read(path).splitlines():
        m = re.match(r"\s*(?:#+|[*+-]|\d+\.)\s+(.*)", line)
        if not m:
            continue
        lead = m.group(1).lstrip(" *")
        if lead and is_emoji(ord(lead[0])) and not 0xFE00 <= ord(lead[0]) <= 0xFE0F:
            hexa = f"{ord(lead[0]):x}"
            if not os.path.exists(f"{ROOT}/resources/img/emoji/{hexa}.png"):
                missing_emoji.setdefault(hexa, (lead[0], os.path.relpath(path, ROOT)))
for hexa, (ch, where) in sorted(missing_emoji.items()):
    warnings.append(f"{where}: {ch} has no resources/img/emoji/{hexa}.png (see its README)")

# docs/config.md and the keys config.cpp reads and writes. The page's keys
# are the first cell of its tables and its "## `key`" headings.
config_cpp = read(f"{ROOT}/source/util/config.cpp")
config_md = read(f"{ROOT}/docs/config.md")
code_read = set(re.findall(r'read_\w+\(\s*j\s*,\s*"(\w+)"', config_cpp)) | \
    set(re.findall(r'\bj\.find\(\s*"(\w+)"', config_cpp))
code_written = set(re.findall(r'\b(?:j|st)\["(\w+)"\]', config_cpp))
documented = set(re.findall(r"^\|\s*`(\w+)`\s*\|", config_md, re.M)) | \
    set(re.findall(r"^#+\s*`(\w+)`\s*$", config_md, re.M))
if not config_cpp or not documented:
    errors.append("source/util/config.cpp or docs/config.md not found, or no key found in it")
for k in sorted(documented - code_read - code_written):
    warnings.append(f"docs/config.md documents `{k}`, which source/util/config.cpp neither reads nor writes")
for k in sorted((code_read | code_written) - documented):
    warnings.append(f"source/util/config.cpp keeps `{k}`, which docs/config.md does not document")
for k in sorted(code_written - code_read - {"schema"}):
    warnings.append(f"source/util/config.cpp writes `{k}` but never reads it back")
for k in sorted(code_read - code_written):
    warnings.append(f"source/util/config.cpp reads `{k}` but never writes it (lost at the next save)")

# The contributor pages against the code: the simulator's PLAYSHELF_SIM_*
# variables (read with sim_env, sim_env_flag, sim_env_int, getenv or
# sim_net::value) against docs/contributing/simulator.md's table, and the
# smoke scenarios of tools/desktop_smoke.py (SCENARIO == "name") against the
# list of docs/contributing/tests.md.
SIMULATOR_PAGE, TESTS_PAGE = "docs/contributing/simulator.md", "docs/contributing/tests.md"
simulator_page = read(f"{ROOT}/{SIMULATOR_PAGE}")
tests_page = read(f"{ROOT}/{TESTS_PAGE}")
sim_read = set()
for path, text in code_text.items():
    sim_read |= set(re.findall(r'(?:sim_env(?:_flag|_int)?|getenv|sim_net::value)\(\s*"(PLAYSHELF_SIM_\w+)"', text))
for path in glob.glob(f"{ROOT}/source/**/*.[ch]", recursive=True):
    sim_read |= set(re.findall(r'(?:sim_env(?:_flag|_int)?|getenv)\(\s*"(PLAYSHELF_SIM_\w+)"', read(path)))
# Aurore's Cadran reads its flags through ui::env_flag("NAME"), which
# activity/screen_hooks.cpp answers with PLAYSHELF_SIM_NAME.
for path in glob.glob(f"{ROOT}/library/aurore/library/lib/cadran/**/*.cpp", recursive=True):
    sim_read |= {"PLAYSHELF_SIM_" + n for n in re.findall(r'env_flag\(\s*"(\w+)"', read(path))}
sim_documented = set(re.findall(r"`(PLAYSHELF_SIM_\w+)", simulator_page))
if not sim_read or not sim_documented:
    errors.append(f"no PLAYSHELF_SIM_* variable found in the code or in {SIMULATOR_PAGE}")
for v in sorted(sim_read - sim_documented):
    warnings.append(f"the code reads {v}, which {SIMULATOR_PAGE} does not document (the simulator's table)")
for v in sorted(sim_documented - sim_read):
    warnings.append(f"{SIMULATOR_PAGE} documents {v}, which the code does not read")
scenarios = set(re.findall(r'SCENARIO\s*==\s*"(\w+)"', read(f"{ROOT}/tools/desktop_smoke.py")))
m = re.search(r"desktop_smoke\.py\s+<out-dir>\s+\[([\w|]+)\]", tests_page)
listed_scenarios = set(m.group(1).split("|")) if m else set()
# Each scenario of that synopsis has its line in the list under it.
described = set(re.findall(r"^  - `(\w+)` —", tests_page, re.M))
if not scenarios or not listed_scenarios:
    errors.append(f"no smoke scenario found in tools/desktop_smoke.py, or no list of them in {TESTS_PAGE}")
for v in sorted(scenarios - listed_scenarios):
    warnings.append(f"tools/desktop_smoke.py has the scenario {v}, which {TESTS_PAGE} does not list")
for v in sorted(listed_scenarios - scenarios):
    warnings.append(f"{TESTS_PAGE} lists the smoke scenario {v}, which tools/desktop_smoke.py does not have")
for v in sorted(listed_scenarios - described):
    warnings.append(f"{TESTS_PAGE} lists the smoke scenario {v} but has no line saying what it covers")

# The documentation the app links to (app::doc_url): the page, and the
# heading's anchor as GitHub makes it. A translation answers to the English
# anchors when it has the English page's sections, in the same order.
def heading_slugs(text):
    return [re.sub(r"[^\w\- ]", "", re.sub(r"<[^>]+>", "", h).strip().lower()).replace(" ", "-")
            for h in re.findall(r"^#{2,3} (.+?)\s*$", text, re.M)]


doc_links = set()
for path, text in sorted(code_text.items()):
    rel = os.path.relpath(path, ROOT)
    for page, anchor in re.findall(r'doc_url\(\s*"([\w-]+)"\s*(?:,\s*"([^"]*)")?\s*\)', text):
        doc_links.add((page, anchor, rel))
    if "/blob/main/docs/" in text:
        errors.append(f"{rel} links to docs/ by itself: app::doc_url() makes the link")
for page, anchor, rel in sorted(doc_links):
    english = read(f"{ROOT}/docs/{page}.md")
    if not english:
        errors.append(f"{rel} links to the page {page}, which docs/{page}.md is not")
        continue
    slugs = heading_slugs(english)
    if anchor and anchor not in slugs:
        errors.append(f"{rel} links to {page}#{anchor}, a heading docs/{page}.md does not have "
                      "(an installed app keeps the link: keep the heading, or its anchor)")
        continue
    for translation in sorted(glob.glob(f"{ROOT}/docs/*/{page}.md")):
        if anchor and len(heading_slugs(read(translation))) != len(slugs):
            warnings.append(f"{os.path.relpath(translation, ROOT)} has not the sections of docs/{page}.md: "
                            f"the app's link to #{anchor} opens the page, not the section")

# What a build ships must be free to ship: no proprietary system font (its
# name table names its maker; a WOFF2's is compressed, so only the others are
# read), no key file, no game package or part of one.
FONT_MAKERS = ["Nintendo", "Morisawa"]
FONT_EXTENSIONS = (".ttf", ".otf", ".ttc", ".woff")
GAME_EXTENSIONS = (".nsp", ".nsz", ".xci", ".xcz", ".nca", ".ncz", ".tik")
KEY_FILES = re.compile(r"\.keys$|^(PRODINFO|BOOT0|BOOT1)(\.bin)?$", re.I)
# The whole tree, Aurore's submodule (library/aurore) included, but git's
# data and the build folders (build, build-web...).
for folder, dirs, files in os.walk(ROOT):
    dirs[:] = [d for d in dirs if d not in (".git", "node_modules") and not d.startswith("build")]
    for name in files:
        path = os.path.join(folder, name)
        rel = os.path.relpath(path, ROOT)
        lower = name.lower()
        if lower.endswith(FONT_EXTENSIONS):
            data = open(path, "rb").read()
            for maker in FONT_MAKERS:
                if maker.encode() in data or maker.encode("utf-16-be") in data:
                    errors.append(f"{rel} names {maker}: a proprietary font, which no build may ship")
        if lower.endswith(GAME_EXTENSIONS):
            errors.append(f"{rel}: a game package or a part of one, which the repository never carries")
        if KEY_FILES.search(name):
            errors.append(f"{rel}: a console's key file, which the repository never carries")

for w in warnings:
    print("WARNING:", w)
for e in errors:
    print("ERROR:", e)
print(f"resources check: {len(errors)} error(s)")
sys.exit(1 if errors else 0)
