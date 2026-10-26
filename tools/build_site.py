#!/usr/bin/env python3
"""Builds PlayShelf's website (site/README.md) into <out-dir>: a home page and
the documentation (docs/*.md, docs/<language>/*.md) in each language the site
has texts for (site/i18n/<language>.json, of the console's 15), English at the
root and the others in /<language>/.

- The home page's texts are site/i18n/<language>.json (en-US.json the
  reference; a text missing elsewhere is shown in English). `{{app:<key>}}`
  in them is the app's own text for that key, in that language
  (resources/i18n/<language>/playshelf.json), so a menu named on the site is
  named as on the console; `{{version}}` the latest release
  (.release-please-manifest.json).
- The documentation is the Markdown of docs/: a language reads
  docs/<language>/ where it has the page (Canadian French reads docs/fr/),
  else the English one, with a note saying so. A translation starts with
  `<!-- translated from docs/<page>.md, sha256 <12 hex> -->`
  (tools/doc_stamp.py writes it): when the English page no longer has that
  hash, the page says that the English one has changed since. Links between the docs and to the README become the
  site's pages; other files of the repository link to GitHub.
- The screenshots are the console's in each language when SITE_SCREENSHOTS
  names the folder tools/site_screenshots.mjs wrote them to (CI), else the
  English ones of images/screenshots/.
- For search engines and AI assistants: absolute links (SITE_URL, the site's
  address), a sitemap (and robots.txt at a domain's root), structured data (JSON-LD), and each page of the
  documentation in Markdown next to it, listed in llms.txt (llms-full.txt has
  them all in one file). SITE_NOINDEX=1 keeps a pull request's preview out of
  search engines.
- The simulator (web/README.md) is not built here: the deployment
  (.github/workflows/pages.yml) puts it in /simulator/.

Usage: tools/build_site.py <out-dir> [repo-root]          (needs Python-Markdown)
"""
import gzip
import hashlib
import html
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit

import markdown

REPO_URL = "https://github.com/JigSawFr/PlayShelf"
# The site's address, for the links that must be absolute (the canonical
# page, the other languages, the preview image, the sitemap, llms.txt).
SITE_URL = os.environ.get("SITE_URL", "https://jigsawfr.github.io/PlayShelf/").rstrip("/") + "/"
# A pull request's preview: not for search engines (main's site is the one).
NOINDEX = os.environ.get("SITE_NOINDEX") == "1"
# The screenshots of tools/site_screenshots.mjs: <folder>/<language>/<screen>.webp.
SCREENSHOT_DIR = os.environ.get("SITE_SCREENSHOTS", "")
# The built simulator (CI's build-web/simulator), to say on its buttons how
# much it downloads; none for a local build.
SIMULATOR_DIR = os.environ.get("SITE_SIMULATOR", "")
# The documentation's "Try it in the simulator" links (main's simulator, as
# GitHub shows them): the site's own simulator on the site, in its language.
SIMULATOR_URL = "https://jigsawfr.github.io/PlayShelf/simulator/"
# The console's languages: config.json's code, the site's folder ('' for the
# root), the name in its own words, the html lang. The site is built in those
# site/i18n/ has a catalog for (main()).
LANGUAGES = [
    ("en-US", "", "English", "en"),
    ("fr", "fr", "Français", "fr"),
    ("fr-CA", "fr-ca", "Français (Canada)", "fr-CA"),
    ("de", "de", "Deutsch", "de"),
    ("es", "es", "Español", "es"),
    ("es-419", "es-419", "Español (Latinoamérica)", "es-419"),
    ("it", "it", "Italiano", "it"),
    ("nl", "nl", "Nederlands", "nl"),
    ("pt", "pt", "Português", "pt-PT"),
    ("pt-BR", "pt-br", "Português (Brasil)", "pt-BR"),
    ("ru", "ru", "Русский", "ru"),
    ("ja", "ja", "日本語", "ja"),
    ("ko", "ko", "한국어", "ko"),
    ("zh-Hans", "zh-hans", "简体中文", "zh-Hans"),
    ("zh-Hant", "zh-hant", "繁體中文", "zh-Hant"),
]
# Open Graph's name of each language.
OG_LOCALES = {"en-US": "en_US", "fr": "fr_FR", "fr-CA": "fr_CA", "de": "de_DE", "es": "es_ES", "es-419": "es_LA",
              "it": "it_IT", "nl": "nl_NL", "pt": "pt_PT", "pt-BR": "pt_BR", "ru": "ru_RU", "ja": "ja_JP",
              "ko": "ko_KR", "zh-Hans": "zh_CN", "zh-Hant": "zh_TW"}
# The documentation, in the sidebar's order.
DOCS = [("getting-started", "guide"), ("servers", "guide"), ("downloads", "guide"), ("troubleshooting", "guide"),
        ("config", "reference")]
# The documentation's folder of a language (docs/<folder>/), when not its own code.
DOC_FOLDERS = {"fr-CA": "fr"}
# The pages' Content-Security-Policy (GitHub Pages sends no headers): only the
# site's own files, no inline script, nothing from elsewhere.
CSP = ("default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; media-src 'self'; "
       "font-src 'self'; connect-src 'self'; object-src 'none'; base-uri 'none'; form-action 'none'")
# The gallery's screens (tools/site_screenshots.py), the first one the home page's.
SCREENSHOTS = ["catalogue", "game_sheet", "downloads", "library", "preferences"]
# The screenshots in each language: the gallery's, which the documentation shows too.
LOCALIZED_SHOTS = SCREENSHOTS
FAQ = ["what", "own", "servers", "tinfoil", "https", "cfw", "switch2", "free"]
FEATURES = ["catalogue", "downloads", "security", "lan", "titles", "library", "updates", "languages"]
# The three the home page puts first; the others in a shorter list under them.
FEATURED = ["catalogue", "downloads", "security"]
# Each feature's icon: a line drawing on a 24px grid, in the text's colour.
ICONS = {
    "catalogue": '<rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/>'
                 '<rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/>',
    "downloads": '<path d="M12 3v12M7 10l5 5 5-5"/><path d="M4 19h16"/>',
    "security": '<path d="M12 3 5 6v5c0 4.5 3 8.3 7 10 4-1.7 7-5.5 7-10V6z"/><path d="m9 12 2 2 4-4"/>',
    "lan": '<path d="M5 12.5a10 10 0 0 1 14 0M8 15.5a5.5 5.5 0 0 1 8 0"/><circle cx="12" cy="19" r="1"/>'
           '<path d="M2 9a14.5 14.5 0 0 1 20 0"/>',
    "titles": '<path d="M4 5h16M4 10h16M4 15h10M4 20h7"/>',
    "library": '<path d="M4 20V5M9 20V5M14 20 17 5l3.5.7-3 15"/><path d="M3 20h18"/>',
    "updates": '<path d="M20 12a8 8 0 1 1-2.3-5.6"/><path d="M20 4v4h-4"/>',
    "languages": '<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3a14 14 0 0 1 0 18 14 14 0 0 1 0-18"/>',
}


def icon(name):
    return (f'<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" '
            f'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">{ICONS[name]}</svg>')

ROOT = "."
APP_TEXTS = {}


def flat(d, prefix=""):
    out = {}
    for k, v in d.items():
        if isinstance(v, dict):
            out.update(flat(v, f"{prefix}{k}/"))
        else:
            out[f"{prefix}{k}"] = v
    return out


def read_json(*parts):
    with open(os.path.join(ROOT, *parts), encoding="utf-8") as f:
        return json.load(f)


def app_text(lang, key):
    if lang not in APP_TEXTS:
        APP_TEXTS[lang] = flat(read_json("resources", "i18n", lang, "playshelf.json"))
    value = APP_TEXTS[lang].get(key) or flat(read_json("resources", "i18n", "en-US", "playshelf.json")).get(key)
    if value is None:
        raise SystemExit(f"site: no app text {key!r}")
    return value.rstrip(".…").rstrip()   # "Find on this network…" as a menu's name


class Texts:
    """The home page's texts in one language, English underneath."""

    def __init__(self, lang, version):
        self.lang, self.version = lang, version
        self.english = read_json("site", "i18n", "en-US.json")
        path = os.path.join(ROOT, "site", "i18n", f"{lang}.json")
        self.own = read_json("site", "i18n", f"{lang}.json") if os.path.exists(path) else {}

    def __call__(self, key, escape=True):
        value = self.own.get(key, self.english.get(key))
        if value is None:
            raise SystemExit(f"site: no text {key!r} in site/i18n/en-US.json")
        value = value.replace("{{version}}", self.version)
        value = re.sub(r"\{\{app:([^}]+)\}\}", lambda m: app_text(self.lang, m.group(1)), value)
        return html.escape(value) if escape else value


# ---- The documentation's versions ----------------------------------------

class DocSet:
    """The documentation being built: main's (from the repository), or the
    latest release's (from the files of its tag, its own screenshots under
    assets/<tag>/), at `prefix`: docs/ (the site's, the latest release's
    where it has the page in the language, main's otherwise), docs/dev/,
    docs/<tag>/."""

    def __init__(self, root, pages, tag=None, prefix=None):
        self.root, self.pages, self.tag = root, pages, tag
        self.prefix = prefix or (f"docs/{tag}/" if tag else "docs/")

    def asset(self, path):
        return f"{self.tag}/{path}" if self.tag else path


DEV_DOCS = DocSet(".", [])
LATEST_TAG = None        # the latest release's tag, when git has it
DOCSET = DEV_DOCS        # the one being built
RELEASE_DOCS = None      # the latest release's, when git has its tag
RELEASE_FILES = {}       # the release's images: their path under assets/ -> their file
UNRELEASED = []          # [(section, [(scope, subject, pull request)])]: main since the release
CHANGED = set()          # main's pages that are not the release's: new, or their English changed
NEW_SECTIONS = {}        # page -> the places (in its sections' order) of main's sections the release had not


def git(*args):
    """git's output in the repository, or None (no git, no history: a tarball,
    a shallow clone)."""
    try:
        return subprocess.run(["git", "-C", ROOT, *args], capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None


def latest_tag():
    """The latest release's tag reachable from here (v1.2.0), or None."""
    out = git("describe", "--tags", "--abbrev=0", "--match", "v[0-9]*", "HEAD")
    return out.strip() if out else None


def release_docs(tag, into):
    """The release's documentation, its Markdown and screenshots out of its
    tag into `into`: a DocSet of the pages it had, or None."""
    listing = git("ls-tree", "-r", "--name-only", tag, "docs", "images/screenshots")
    if not listing:
        return None
    root = os.path.realpath(into)
    for path in listing.splitlines():
        target = os.path.realpath(os.path.join(root, path))
        if not target.startswith(root + os.sep):   # nothing outside `into`
            continue
        try:
            data = subprocess.run(["git", "-C", ROOT, "show", f"{tag}:{path}"], capture_output=True, check=True).stdout
        except (OSError, subprocess.CalledProcessError):
            return None
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "wb") as f:
            f.write(data)
    pages = [(p, s) for p, s in DOCS if os.path.exists(os.path.join(into, "docs", f"{p}.md"))]
    return DocSet(into, pages, tag) if pages else None


# A commit's subject: "feat(console): protect a forwarder with the PIN (#263)".
COMMIT = re.compile(r"^(\w+)(?:\(([^)]*)\))?!?: (.+?)(?: \(#(\d+)\))?$")


def unreleased(tag):
    """What main has since `tag`, as the next release's notes will say it
    (release-please-config.json's sections, its hidden types left out)."""
    out = git("log", "--first-parent", "--format=%s", f"{tag}..HEAD")
    if not out:
        return []
    config = read_json("release-please-config.json")
    sections = {s["type"]: s["section"] for s in config["changelog-sections"] if not s.get("hidden")}
    order = list(dict.fromkeys(sections.values()))
    grouped = {}
    for subject in out.splitlines():
        m = COMMIT.match(subject)
        if not m or m.group(1) not in sections or subject.startswith("chore(main): release"):
            continue
        grouped.setdefault(sections[m.group(1)], []).append((m.group(2) or "", m.group(3), m.group(4)))
    return [(s, grouped[s]) for s in order if s in grouped]


def english_headings(root, page_name):
    """The sections of docs/<page>.md under `root`, in order (their anchors)."""
    with open(os.path.join(root, "docs", f"{page_name}.md"), encoding="utf-8") as f:
        return [github_slug(m) for m in re.findall(r"^#{2,3} (.+?)\s*$", f.read(), re.M)]


def compare_release():
    """Which of main's pages are not the release's (new, or their English
    changed), and in those the release had, which sections are new."""
    for page_name, _ in DOCS:
        if page_name not in dict(RELEASE_DOCS.pages):
            CHANGED.add(page_name)
            continue
        with open(os.path.join(DEV_DOCS.root, "docs", f"{page_name}.md"), encoding="utf-8") as f, \
                open(os.path.join(RELEASE_DOCS.root, "docs", f"{page_name}.md"), encoding="utf-8") as g:
            if f.read().rstrip() == g.read().rstrip():
                continue
        CHANGED.add(page_name)
        had = set(english_headings(RELEASE_DOCS.root, page_name))
        NEW_SECTIONS[page_name] = {i for i, h in enumerate(english_headings(DEV_DOCS.root, page_name)) if h not in had}


def release_serves(page_name, lang):
    """Whether the site's page (docs/<page>/) in `lang` is the latest
    release's: it had the page, written in that language."""
    if not RELEASE_DOCS or page_name not in dict(RELEASE_DOCS.pages):
        return False
    return lang == "en-US" or os.path.exists(os.path.join(RELEASE_DOCS.root, "docs", doc_folder(lang), f"{page_name}.md"))


# ---- Markdown ------------------------------------------------------------

def github_slug(value, separator="-"):
    """The anchor GitHub gives a heading, so links written for GitHub still work."""
    s = html.unescape(re.sub(r"<[^>]+>", "", value)).strip().lower()
    s = re.sub(r"[^\w\- ]", "", s)
    return s.replace(" ", separator)


ALERTS = {"NOTE": "note", "TIP": "tip", "IMPORTANT": "important", "WARNING": "warning", "CAUTION": "caution"}


def alerts(body):
    """GitHub's "> [!WARNING]" blockquotes as callouts."""
    def callout(m):
        kind = ALERTS[m.group(1)]
        return f'<div class="callout {kind}">\n<p>'
    body = re.sub(r"<blockquote>\s*<p>\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]\s*(?:<br\s*/?>)?\s*", callout, body)
    # The blockquotes opened as callouts close as divs.
    out, stack = [], []
    for token in re.split(r"(<blockquote>|</blockquote>|<div class=\"callout [a-z]+\">)", body):
        if token == "<blockquote>" or token.startswith("<div class=\"callout"):
            stack.append(token)
            out.append(token)
        elif token == "</blockquote>":
            opened = stack.pop() if stack else "<blockquote>"
            out.append("</div>" if opened.startswith("<div") else token)
        else:
            out.append(token)
    return "".join(out)


def rewrite_links(body, source, lang, to_root, assets):
    """Links of a document of the repository (`source`, its path) as the site's."""
    folder = os.path.dirname(source)

    def target(url):
        parts = urlsplit(url)
        if url.startswith(SIMULATOR_URL) and not parts.fragment:
            query = "&".join(q for q in [parts.query, f"lang={lang}"] if q)
            return f"{to_root}simulator/?{query}"
        if parts.scheme or url.startswith(("#", "mailto:")):
            return url
        path = os.path.normpath(os.path.join(folder, parts.path)).replace(os.sep, "/")
        anchor = f"#{parts.fragment}" if parts.fragment else ""
        name = os.path.basename(path)
        if path.startswith("docs/") and name.endswith(".md") and name[:-3] in dict(DOCSET.pages):
            # From main's page on docs/, to a page docs/ shows as the
            # release's: main's, at docs/dev/ (its sections, its anchors).
            prefix = DOCSET.prefix
            if prefix == "docs/" and not DOCSET.tag and release_serves(name[:-3], lang):
                prefix = "docs/dev/"
            return f"{to_root}{lang_dir(lang)}{prefix}{name[:-3]}/{anchor}"
        if path == "README.md":
            return f"{to_root}{lang_dir(lang)}{anchor}"
        # The console's in the page's language: main's only (a release keeps its own).
        if not DOCSET.tag and path.startswith("images/screenshots/") and name[:-4] in LOCALIZED_SHOTS:
            path = screenshot(lang, name[:-4])
        if path.startswith("images/"):
            asset = DOCSET.asset(path)
            if DOCSET.tag:
                RELEASE_FILES[asset] = os.path.join(DOCSET.root, path)
            assets.add(asset)
            return f"{to_root}assets/{asset}"
        return f"{REPO_URL}/blob/{DOCSET.tag or 'main'}/{path}{anchor}"

    body = re.sub(r'(href|src)="([^"]+)"', lambda m: f'{m.group(1)}="{html.escape(target(html.unescape(m.group(2))))}"', body)
    return body


def lang_dir(lang):
    folder = next(d for code, d, _, _ in LANGUAGES if code == lang)
    return f"{folder}/" if folder else ""


def screenshot(lang, name, dark=False):
    """The screenshot `name` under assets/: the console's in `lang` when
    SITE_SCREENSHOTS has it, else the English one of images/screenshots/.
    With `dark`, its dark theme's, or None."""
    file = f"{name}-dark.webp" if dark else f"{name}.webp"
    if SCREENSHOT_DIR and os.path.exists(os.path.join(SCREENSHOT_DIR, lang, file)):
        return f"images/screenshots/{lang}/{file}"
    return None if dark else f"images/screenshots/{name}.png"


def social_image(lang):
    """The preview image of `lang`'s pages (og:image) under assets/: its own
    when SITE_SCREENSHOTS has it, else the repository's
    (.github/readme/social-preview.png, published as this path)."""
    if SCREENSHOT_DIR and os.path.exists(os.path.join(SCREENSHOT_DIR, lang, "social.png")):
        return f"images/screenshots/{lang}/social.png"
    return "images/social-preview.png"


def gallery_image(t, lang, to_root, name, lazy=True):
    """A screenshot of the gallery, in the page's theme: its dark one under
    the dark theme (site.css), when there is one. The home page's first one
    is not lazy: it is in view at once."""
    light, dark = screenshot(lang, name), screenshot(lang, name, dark=True)
    # A sentence saying what the screen shows (its name is the caption).
    alt = t(f"shot_{name}_alt")
    loading = ' loading="lazy"' if lazy else ""
    img = f'<img src="{to_root}assets/{light}" alt="{alt}" width="1280" height="720"{loading}'
    if not dark:
        return img + ">"
    return (img + ' class="shot-light">'
            f'<img src="{to_root}assets/{dark}" alt="{alt}" width="1280" height="720"{loading} class="shot-dark">')


def back_to_readme(text):
    """Without the docs' "Back to the README." (GitHub's way back, not the
    site's): the sentence that ends with the link to the README, in any language."""
    return re.sub(r"[ \u3000]?[^.!?。！？\n]*\[README\]\([^)]*\)[.。]?", "", text, count=1)


TRANSLATED = re.compile(r"<!-- translated from (docs/[\w-]+\.md), sha256 ([0-9a-f]{12}) -->")


def english_hash(source):
    with open(os.path.join(DOCSET.root, source), "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()[:12]


def plain(html_text):
    """The text of some HTML, on one line."""
    return re.sub(r"\s+", " ", html.unescape(re.sub(r"<[^>]+>", "", html_text))).strip()


def doc_folder(lang):
    """docs/<folder>/: the documentation in `lang`, where it has the page."""
    return DOC_FOLDERS.get(lang, lang)


def doc_source(page, lang):
    """The Markdown `lang` reads for docs/<page>.md, and whether it is in that language."""
    folder = doc_folder(lang)
    if lang != "en-US" and os.path.exists(os.path.join(DOCSET.root, "docs", folder, f"{page}.md")):
        return f"docs/{folder}/{page}.md", True
    return f"docs/{page}.md", lang == "en-US"


def outdated(source):
    """Whether a translation's English page changed since it was translated."""
    with open(os.path.join(DOCSET.root, source), encoding="utf-8") as f:
        mark = TRANSLATED.search(f.readline())
    return bool(mark) and english_hash(mark.group(1)) != mark.group(2)


def render_doc(page, lang, to_root, assets):
    """(title, html, toc, translated, source path) of docs/<page>.md for `lang`."""
    source, translated = doc_source(page, lang)
    with open(os.path.join(DOCSET.root, source), encoding="utf-8") as f:
        text = back_to_readme(f.read())
    md = markdown.Markdown(extensions=["tables", "fenced_code", "sane_lists", "toc"],
                           extension_configs={"toc": {"slugify": github_slug, "toc_depth": "2-3"}})
    body = md.convert(text)
    title_match = re.search(r"<h1[^>]*>(.*?)</h1>", body, re.S)
    title = re.sub(r"<[^>]+>", "", title_match.group(1)) if title_match else page
    body = re.sub(r"<h1[^>]*>.*?</h1>", "", body, count=1, flags=re.S)
    body = alerts(rewrite_links(body, source, lang, to_root, assets))
    # A paragraph for GitHub only (<!-- github-only --> before it): what the
    # site shows otherwise, such as screenshots in the page's language.
    body = re.sub(r"<!-- github-only[^>]*-->\s*<p>.*?</p>\s*", "", body, flags=re.S)
    body = re.sub(r"<table>.*?</table>", screenshot_table, body, flags=re.S)
    body = re.sub(r"<table>", '<div class="table"><table>', body).replace("</table>", "</table></div>")
    body = re.sub(r"<img [^>]*>", lambda m: dark_twin(m.group(0), lang, assets), body)
    return html.unescape(title), body, md.toc_tokens, translated, source


def dark_twin(img, lang, assets):
    """A screenshot of the documentation in the page's theme: its dark one
    beside it under the dark theme (site.css), as the gallery's, when there
    is one."""
    src = re.search(r'src="([^"]*)"', img)
    name = re.search(r"images/screenshots/[^/]+/(\w+)\.webp$", src.group(1)) if src else None
    dark = screenshot(lang, name.group(1), dark=True) if name else None
    if not dark:
        return img
    assets.add(dark)
    twin = img.replace(src.group(1), src.group(1).rsplit("images/", 1)[0] + dark)
    return img.replace("<img ", '<img class="shot-light" ', 1) + twin.replace("<img ", '<img class="shot-dark" ', 1)


def screenshot_table(m):
    """A table of screenshots under their names (a row of names, a row of
    images, and so on): a grid of figures, each image with its name, which a
    phone shows one under the other. Any other table as it is."""
    rows = [re.findall(r"<t[hd][^>]*>(.*?)</t[hd]>", row, re.S)
            for row in re.findall(r"<tr>(.*?)</tr>", m.group(0), re.S)]
    pairs = list(zip(rows[0::2], rows[1::2]))
    if len(rows) < 2 or len(rows) % 2 or not all(
            len(names) == len(images) and all(re.fullmatch(r"\s*<img [^>]*>\s*", c) for c in images)
            for names, images in pairs):
        return m.group(0)
    figures = "\n".join(f"<figure>{image.strip()}<figcaption>{name.strip()}</figcaption></figure>"
                       for names, images in pairs for name, image in zip(names, images))
    return f'<div class="shots">\n{figures}\n</div>'


# ---- Pages ------------------------------------------------------------

def json_ld(data):
    """Structured data for search engines and AI assistants (schema.org): a
    data block, not a script, so the Content-Security-Policy lets it be."""
    text = json.dumps(data, ensure_ascii=False, indent=1).replace("</", "<\\/")
    return f'  <script type="application/ld+json">\n{text}\n  </script>\n'


def page(t, lang, to_root, path, title, body, description=None, body_class="", full_title=None, structured=(),
         markdown_url=None, indexed=True):
    """A whole page: `path` is its place under the language's folder ('' or 'docs/x/')."""
    code, folder, _, html_lang = next(l for l in LANGUAGES if l[0] == lang)
    description = description or t("meta_description")
    head = []
    if indexed:
        url = f"{SITE_URL}{lang_dir(lang)}{path}"
        head.append(f'  <link rel="canonical" href="{url}">')
        head += [f'  <link rel="alternate" hreflang="{hl}" href="{SITE_URL}{f"{d}/" if d else ""}{path}">'
                 for _, d, _, hl in LANGUAGES]
        head.append(f'  <link rel="alternate" hreflang="x-default" href="{SITE_URL}{path}">')
        head.append(f'  <meta property="og:url" content="{url}">')
    if markdown_url:
        head.append(f'  <link rel="alternate" type="text/markdown" href="{markdown_url}">')
    if NOINDEX or not indexed:
        head.append('  <meta name="robots" content="noindex">')
    options = "\n".join(
        f'        <option value="{to_root}{f"{d}/" if d else ""}{path}" data-lang="{c}" lang="{hl}"'
        f'{" selected" if c == lang else ""}>{html.escape(n)}</option>'
        for c, d, n, hl in LANGUAGES)
    home = f"{to_root}{lang_dir(lang)}"
    full_title = full_title or ("PlayShelf" if not title else f"{title} · PlayShelf")
    head_extra = "\n".join(head)
    return f"""<!doctype html>
<html lang="{html_lang}" data-lang="{code}">
<head>
  <meta charset="utf-8">
  <meta http-equiv="Content-Security-Policy" content="{CSP}">
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
  <title>{html.escape(full_title)}</title>
  <meta name="description" content="{description}">
{head_extra}
  <meta property="og:type" content="website">
  <meta property="og:site_name" content="PlayShelf">
  <meta property="og:locale" content="{OG_LOCALES[code]}">
  <meta property="og:title" content="{html.escape(full_title)}">
  <meta property="og:description" content="{description}">
  <meta property="og:image" content="{SITE_URL}assets/{social_image(code)}">
  <meta property="og:image:width" content="1280">
  <meta property="og:image:height" content="640">
  <meta property="og:image:alt" content="PlayShelf">
  <meta name="twitter:card" content="summary_large_image">
  <meta name="theme-color" content="#ffffff" media="(prefers-color-scheme: light)">
  <meta name="theme-color" content="#111d31" media="(prefers-color-scheme: dark)">
  <link rel="icon" href="{to_root}assets/icon.svg" type="image/svg+xml">
  <link rel="apple-touch-icon" href="{to_root}assets/apple-touch-icon.png">
  <meta name="apple-mobile-web-app-title" content="PlayShelf">
  <meta name="format-detection" content="telephone=no">
  <meta name="application-name" content="PlayShelf">
  <link rel="manifest" href="{to_root}assets/site.webmanifest">
  <link rel="sitemap" type="application/xml" href="{SITE_URL}sitemap.xml">
  <link rel="alternate" type="application/atom+xml" title="PlayShelf releases" href="{SITE_URL}releases.atom">
{"".join(json_ld(d) for d in structured)}  <script src="{to_root}assets/theme.js"></script>
  <link rel="stylesheet" href="{to_root}assets/site.css">
</head>
<body class="{body_class}">
  <a class="skip" href="#main">{t('skip')}</a>
  <header class="top">
    <a class="brand" href="{home}"><img src="{to_root}assets/mark.svg" alt="" width="34" height="34"><span>Play<b>Shelf</b></span></a>
    <nav class="nav" aria-label="PlayShelf">
      <a href="{home}#features">{t('nav_features')}</a>
      <a href="{home}#install">{t('nav_install')}</a>
      <a href="{home}docs/getting-started/">{t('nav_docs')}</a>
      <a href="{to_root}simulator/?lang={code}" class="nav-try">{t('nav_simulator')}</a>
      <a href="{REPO_URL}" class="nav-gh">GitHub</a>
    </nav>
    <div class="settings">
      <span class="picker picker-lang"><span class="picker-face" aria-hidden="true">{code.split("-")[0].upper()}</span>
      <select id="site-language" aria-label="{t('language')}">
{options}
      </select></span>
      <span class="picker picker-theme"><span class="picker-face" aria-hidden="true"></span>
      <select id="site-theme" aria-label="{t('theme')}">
        <option value="auto">{t('theme_auto')}</option>
        <option value="light">{t('theme_light')}</option>
        <option value="dark">{t('theme_dark')}</option>
      </select></span>
    </div>
  </header>
  <main id="main">
{body}
  </main>
  <footer class="footer">
    <div class="footer-inner">
      <div class="footer-cols">
        <div>
          <p><img src="{to_root}assets/mark.svg" alt="" width="20" height="20"> <strong>PlayShelf</strong></p>
          <p>{t('footer_license')}</p>
        </div>
        <div>
          <h2>{t('docs_title')}</h2>
          <ul>{"".join(f'<li><a href="{home}docs/{p}/">{t(f"doc_{p}")}</a></li>' for p, s in DOCS if s == "guide")}</ul>
        </div>
        <div>
          <h2>PlayShelf</h2>
          <ul><li><a href="{REPO_URL}">GitHub</a></li><li><a href="{home}changelog/">{t('footer_changelog')}</a></li>
            <li><a href="{SITE_URL}releases.atom">{t('releases_feed')}</a></li>
            <li><a href="{REPO_URL}/issues/new">{t('report_bug')}</a></li></ul>
        </div>
        <div>
          <h2>{t('support')}</h2>
          <ul><li><a href="https://github.com/sponsors/JigSawFr">GitHub Sponsors</a></li><li><a href="https://ko-fi.com/jigsawfr">Ko-fi</a></li></ul>
        </div>
      </div>
      <p class="fine">{t('footer_nintendo')}</p>
    </div>
  </footer>
  <dialog class="lightbox" aria-label="{t('screens_title')}">
    <figure><img alt=""><figcaption></figcaption></figure>
    <button type="button" class="lightbox-close" aria-label="{t('close')}"></button>
    <button type="button" class="lightbox-prev" aria-label="{t('previous')}"></button>
    <button type="button" class="lightbox-next" aria-label="{t('next')}"></button>
  </dialog>
  <a class="to-top" href="#main" aria-label="{t('back_to_top')}" hidden></a>
  <script src="{to_root}assets/site.js"></script>
</body>
</html>
"""


def home_body(t, lang, to_root):
    code = lang
    def feature(key):
        return (f'      <li class="card">{icon(key)}'
                f'<h3>{t(f"f_{key}_title")}</h3><p>{t(f"f_{key}_text")}</p></li>')
    featured = "\n".join(feature(k) for k in FEATURES if k in FEATURED)
    others = "\n".join(feature(k) for k in FEATURES if k not in FEATURED)
    weight = simulator_size()
    size = f' <span class="size" data-bytes="{weight}"></span>' if weight else ""
    shots = "\n".join(
        f'      <figure><a href="{to_root}assets/{screenshot(lang, s)}">{gallery_image(t, lang, to_root, s)}</a>'
        f'<figcaption>{t(f"shot_{s}")}</figcaption></figure>'
        for s in SCREENSHOTS)
    faq = "\n".join(
        f'      <details id="faq-{q}"><summary>{t(f"faq_{q}_q")}</summary><p>{t(f"faq_{q}_a")}</p></details>'
        for q in FAQ)
    docs = docs_list(t, lang, to_root, None)
    home = f"{to_root}{lang_dir(lang)}"
    # The latest release and its date, or none yet: the download is the
    # releases' page until there is one.
    releases = changelog_releases()
    if releases:
        version, date, _ = releases[0]
        released = (f'<a href="{REPO_URL}/releases/latest">{t("download")}</a>'
                    f' · <a href="{home}changelog/#v{version}"><time class="release-date" datetime="{date}">{date}</time></a>')
    else:
        released = t("no_release")
    return f"""    <section class="hero">
      <div class="hero-text">
        <p class="wordmark" aria-hidden="true">Play<span>Shelf</span></p>
        <h1>{t('hero_title')}</h1>
        <p class="sub">{t('hero_sub')}</p>
        <p>{t('hero_text')}</p>
        <div class="actions">
          <a class="button primary" href="{to_root}simulator/?lang={code}">{t('try')}{size}</a>
          <a class="button" href="#install">{t('install_cta')}</a>
        </div>
        <p class="source">{released}</p>
      </div>
      <div class="hero-shot">
        <a href="{to_root}assets/{screenshot(lang, SCREENSHOTS[0])}">{gallery_image(t, lang, to_root, SCREENSHOTS[0], lazy=False)}</a>
      </div>
    </section>

    <aside class="callout warning wide"><p><strong>{t('own_title')}</strong> {t('own_text')}</p></aside>

    <section id="features" class="section">
      <h2>{t('features_title')}</h2>
      <ul class="cards">
{featured}
      </ul>
      <h3 class="more-title">{t('features_more')}</h3>
      <ul class="cards cards-small">
{others}
      </ul>
    </section>

    <section id="screens" class="section">
      <h2>{t('screens_title')}</h2>
      <div class="gallery">
{shots}
      </div>
    </section>

    <section id="simulator" class="section band">
      <div>
        <h2>{t('simulator_title')}</h2>
        <p>{t('simulator_text')}</p>
      </div>
      <a class="button primary" href="{to_root}simulator/?lang={code}">{t('simulator_button')}{size}</a>
    </section>

    <section id="install" class="section columns">
      <div>
        <h2>{t('install_title')}</h2>
        <ol class="steps">
          <li>{t('install_1')}</li>
          <li>{t('install_2')}{f' <a href="{REPO_URL}/releases/latest">{t("download")}</a>' if releases else ""}</li>
          <li>{t('install_3')}</li>
        </ol>
      </div>
      <div>
        <h2>{t('start_title')}</h2>
        <ol class="steps">
          <li>{t('start_1')}</li>
          <li>{t('start_2')}</li>
          <li>{t('start_3')}</li>
        </ol>
        <p><a href="{home}docs/getting-started/">{t('start_link')}</a></p>
      </div>
    </section>

    <section id="faq" class="section">
      <h2>{t('faq_title')}</h2>
      <div class="faq">
{faq}
      </div>
    </section>

    <section id="docs" class="section">
      <h2>{t('docs_title')}</h2>
{docs}
    </section>
"""


def simulator_size():
    """What a first visit to the simulator downloads, compressed as GitHub
    Pages sends it (bytes; 0 without a built simulator): PlayShelf, its data
    and its script. The CJK font, for Japanese, Korean and Chinese only, is not
    counted."""
    total = 0
    for name in ("playshelf.wasm", "playshelf.data", "playshelf.js"):
        path = os.path.join(SIMULATOR_DIR, name)
        if not SIMULATOR_DIR or not os.path.isfile(path):
            return 0
        with open(path, "rb") as f:
            total += len(gzip.compress(f.read(), 6))
    return total


def docs_search(t, lang, to_root):
    """The documentation's search box: site.js shows it, and searches docs/search.json."""
    return (f'      <div class="doc-search" hidden><input type="search" placeholder="{t("search_placeholder")}"'
            f' aria-label="{t("search_placeholder")}" aria-controls="doc-results" autocomplete="off"'
            f' autocapitalize="off" autocorrect="off" spellcheck="false" enterkeyhint="search"'
            f' data-index="{to_root}{lang_dir(lang)}docs/search.json" data-none="{t("search_none")}">'
            f'<ol id="doc-results" class="doc-results" hidden></ol></div>')


def search_entries(page_name, title, body):
    """The search index' entries of a page: its introduction, then each section (h2)."""
    parts = re.split(r'<h2 id="([^"]*)">(.*?)</h2>', body, flags=re.S)
    entries = [{"p": title, "h": "", "u": f"docs/{page_name}/", "t": plain(parts[0])[:1500]}]
    for i in range(1, len(parts), 3):
        entries.append({"p": title, "h": plain(parts[i + 1]), "u": f"docs/{page_name}/#{parts[i]}",
                        "t": plain(parts[i + 2])[:3000]})
    return entries


def other_entries(t):
    """The search index' entries beside the documentation: the home page's
    questions (what a visitor types) and each release of the changelog,
    their addresses from the language's folder (site.js opens a question or
    an older release a link leads to)."""
    entries = [{"p": t("faq_title", escape=False), "h": t(f"faq_{q}_q", escape=False), "u": f"#faq-{q}",
                "t": t(f"faq_{q}_a", escape=False)} for q in FAQ]
    entries += [{"p": t("footer_changelog", escape=False), "h": version, "u": f"changelog/#v{version}",
                 "t": plain(body)[:3000]} for version, _, body in changelog_releases()]
    return entries


def docs_list(t, lang, to_root, current):
    home = f"{to_root}{lang_dir(lang)}"
    out = []
    for section in ("guide", "reference"):
        items = "".join(
            f'<li><a href="{home}{DOCSET.prefix}{p}/"{" aria-current=\"page\"" if p == current else ""}>{t(f"doc_{p}")}</a></li>'
            for p, s in DOCSET.pages if s == section)
        if not items:
            continue
        out.append(f'      <div class="doc-group"><h3>{t(f"docs_{section}")}</h3><ul>{items}</ul></div>')
    return f'      <div class="doc-list">\n' + "\n".join(out) + "\n      </div>"


def doc_versions(t, lang, to_root, page_name):
    """The documentation's two versions, the latest release's and main's, as
    links to this page in each (to the first page of the release's when it
    did not have this one): (the switch, the release's link, main's link)."""
    if not RELEASE_DOCS:
        return "", "", ""
    home = f"{to_root}{lang_dir(lang)}"
    released = [p for p, _ in RELEASE_DOCS.pages]
    release = f"{home}{RELEASE_DOCS.prefix}{page_name if page_name in released else released[0]}/"
    main_page = f"{home}docs/dev/{page_name}/"
    current = ' aria-current="page"'
    switch = (f'      <nav class="doc-versions" aria-label="{t("doc_version")}">'
              f'<a href="{release}"{current if DOCSET.tag else ""}>{RELEASE_DOCS.tag}</a>'
              f'<a href="{main_page}"{"" if DOCSET.tag else current}>{t("doc_dev")}</a></nav>')
    return switch, release, main_page


def release_text(t, key):
    """A text naming the latest release (its tag, v1.2.0)."""
    return t(key).replace("{{release}}", LATEST_TAG or "")


def section_anchor(page_name, lang, english_id):
    """The anchor in `lang`'s page of the section English calls `english_id`:
    the heading at the same place (a translation keeps the page's sections),
    else the page itself."""
    def headings(source):
        with open(os.path.join(DEV_DOCS.root, source), encoding="utf-8") as f:
            return [github_slug(m) for m in re.findall(r"^#{2,3} (.+?)\s*$", f.read(), re.M)]
    english = headings(f"docs/{page_name}.md")
    source, _ = doc_source(page_name, lang)
    own = headings(source)
    if english_id not in english or len(own) != len(english):
        return ""
    return f"#{own[english.index(english_id)]}"


def doc_body(t, lang, to_root, page_name, assets):
    title, body, toc, translated, source = render_doc(page_name, lang, to_root, assets)
    versions, release_page, main_page = doc_versions(t, lang, to_root, page_name)
    untranslated = release_text(t, "doc_release_untranslated") if DOCSET.tag else t("doc_untranslated")
    note = "" if translated else f'      <p class="callout note">{untranslated}</p>\n'
    if translated and lang != "en-US" and outdated(source):
        note = (f'      <p class="callout note">{t("doc_outdated")} '
                f'<a href="{to_root}{DOCSET.prefix}{page_name}/" hreflang="en">{t("doc_english")}</a></p>\n')
    # Which version this page describes, and the other one: main's says so
    # where it is not the release's (a new page, or one that changed).
    if DOCSET.tag:
        note = (f'      <p class="callout tip">{release_text(t, "doc_release_note")} '
                f'<a href="{main_page}">{t("doc_release_see_dev")}</a></p>\n') + note
    elif RELEASE_DOCS and page_name in CHANGED:
        new_page = page_name not in dict(RELEASE_DOCS.pages)
        links = [] if new_page else [f'<a href="{release_page}">{release_text(t, "doc_dev_see_release")}</a>']
        # What's coming: the changelog's section, there only when main has a
        # change its notes will list (not when it has only hidden ones: ci, chore).
        if UNRELEASED:
            links.append(f'<a href="{to_root}{lang_dir(lang)}changelog/#dev">{t("doc_dev_whats_new")}</a>')
        note = (f'      <p class="callout note">{release_text(t, "doc_dev_new_page" if new_page else "doc_dev_note")} '
                f'{" · ".join(links)}</p>\n') + note
    # The page's sections (its h2: toc_depth leaves the h1 out).
    toc_items = "".join(f'<li><a href="#{h["id"]}">{h["name"]}</a></li>' for h in toc)
    # The same under the page's first paragraph where there is no room for
    # them beside the page (a phone, a tablet): folded.
    shown = body
    if toc_items:
        folded = (f'<details class="toc-mobile"><summary>{t("on_this_page")}</summary>'
                  f'<ul>{toc_items}</ul></details>\n')
        # After the first paragraph when nothing but comments comes before it.
        start = body.find("<p>")
        end = 0
        if start >= 0 and not re.sub(r"<!--.*?-->", "", body[:start], flags=re.S).strip():
            end = body.find("</p>", start) + len("</p>")
            end += body[end:end + 1] == "\n"
        shown = body[:end] + folded + body[end:]
    # main's sections the release had not: "New", in any language (a
    # translation keeps the English page's sections, in the same order).
    new = set() if DOCSET.tag else NEW_SECTIONS.get(page_name, set())
    count = len(re.findall(r'<h[23] id="', shown))
    if new and count != len(english_headings(DEV_DOCS.root, page_name)):
        new = set()
    places = iter(range(count))
    badge = f' <span class="new-badge">{t("doc_new")}</span>'
    # A translation's sections answer to the English anchors too (data-en,
    # site.js): the app's links (app::doc_url) name the English ones.
    english = english_headings(DOCSET.root, page_name) if translated and lang != "en-US" else []
    if len(english) != count:
        english = []

    def heading(m):
        at = next(places)
        alias = f' data-en="{english[at]}"' if english and english[at] != m.group(2) else ""
        return (f'<{m.group(1)} id="{m.group(2)}"{alias}>{m.group(3)}{badge if at in new else ""}'
                f'<a class="anchor" href="#{m.group(2)}"'
                f' aria-label="{t("anchor_link")}"></a></{m.group(1)}>')
    # A link to each section, to share it.
    shown = re.sub(r'<(h[23]) id="([^"]+)">(.*?)</h[23]>', heading, shown, flags=re.S)
    # The pages before and after this one, in the sidebar's order.
    names = [p for p, _ in DOCSET.pages]
    at = names.index(page_name)
    home = f"{to_root}{lang_dir(lang)}"
    pager = "".join(
        f'<a class="{cls}" href="{home}{DOCSET.prefix}{names[i]}/"><span>{t(label)}</span>{t(f"doc_{names[i]}")}</a>'
        for cls, label, i in (("prev", "previous", at - 1), ("next", "next", at + 1)) if 0 <= i < len(names))
    # The page's description: its first paragraph (the one under the title).
    first = re.search(r"<p>(.*?)</p>", body, re.S)
    description = plain(first.group(1)) if first else ""
    if len(description) > 180:
        description = description[:177].rsplit(" ", 1)[0] + "…"
    return title, translated, description, body, f"""    <div class="doc">
      <nav class="doc-nav" aria-label="{t('docs_title')}">
{versions}
{docs_search(t, lang, to_root) if DOCSET.prefix == "docs/" else ""}
{docs_list(t, lang, to_root, page_name)}
      </nav>
      <article class="prose" data-copy="{t('copy')}" data-copied="{t('copied')}">
        <h1>{html.escape(title)}</h1>
{note}{shown}
        <nav class="pager" aria-label="{t('docs_title')}">{pager}</nav>
        <p class="edit"><a href="{REPO_URL}/blob/{DOCSET.tag or 'main'}/{source}">{t('doc_on_github')}</a></p>
      </article>
      {f'<nav class="toc" aria-label="{html.escape(title)}"><ul>{toc_items}</ul></nav>' if toc_items else ''}
    </div>
"""


# ---- For search engines and AI assistants ------------------------------

def markdown_lang(page, lang):
    """The language of the Markdown `lang`'s page of docs/<page>.md is published in."""
    source, _ = doc_source(page, lang)
    folder = source.split("/")[1] if source.count("/") == 2 else ""
    return next((code for code, *_ in LANGUAGES if code == folder), "en-US")


def markdown_langs():
    """The languages the documentation is written in: its Markdown and llms.txt are theirs."""
    return ["en-US"] + [code for code, *_ in LANGUAGES
                        if code != "en-US" and doc_folder(code) == code and os.path.isdir(os.path.join(ROOT, "docs", code))]


def markdown_doc(page, lang, assets):
    """docs/<page>.md as published next to its page, for AI assistants: its
    links absolute, to the site's Markdown, images and pages, else to GitHub."""
    source, _ = doc_source(page, lang)
    with open(os.path.join(ROOT, source), encoding="utf-8") as f:
        text = back_to_readme(f.read())
    folder = os.path.dirname(source)

    def target(m):
        url = m.group(2)
        parts = urlsplit(url)
        if parts.scheme or url.startswith(("#", "mailto:")):
            return m.group(0)
        path = os.path.normpath(os.path.join(folder, parts.path)).replace(os.sep, "/")
        anchor = f"#{parts.fragment}" if parts.fragment else ""
        name = os.path.basename(path)
        if path.startswith("docs/") and name.endswith(".md") and name[:-3] in dict(DOCS):
            url = f"{SITE_URL}{lang_dir(lang)}docs/{name[:-3]}.md{anchor}"
        elif path == "README.md":
            url = f"{SITE_URL}{lang_dir(lang)}{anchor}"
        elif path.startswith("images/"):
            assets.add(path)
            url = f"{SITE_URL}assets/{path}"
        else:
            url = f"{REPO_URL}/blob/main/{path}{anchor}"
        return f"{m.group(1)}{url}{m.group(3)}"

    return re.sub(r"(\]\()([^)\s]+)(\))", target, text)


def software(t, lang, version, url):
    """PlayShelf as schema.org describes an application."""
    return {
        "@context": "https://schema.org",
        "@type": "SoftwareApplication",
        "name": "PlayShelf",
        "url": url,
        "description": t("meta_description", escape=False),
        "applicationCategory": "UtilitiesApplication",
        "applicationSubCategory": "Game library client",
        "operatingSystem": "Nintendo Switch (Atmosphère custom firmware)",
        "softwareVersion": version.lstrip("v"),
        "downloadUrl": f"{REPO_URL}/releases",
        "releaseNotes": f"{REPO_URL}/blob/main/CHANGELOG.md",
        "license": "https://www.gnu.org/licenses/gpl-3.0.html",
        "isAccessibleForFree": True,
        "offers": {"@type": "Offer", "price": "0", "priceCurrency": "USD"},
        "image": f"{SITE_URL}assets/{social_image(lang)}",
        "screenshot": [f"{SITE_URL}assets/{screenshot(lang, s)}" for s in SCREENSHOTS],
        "featureList": [t(f"f_{key}_title", escape=False) for key in FEATURES],
        "inLanguage": [hl for _, _, _, hl in LANGUAGES],
        "author": {"@type": "Person", "name": "JigSawFr", "url": "https://github.com/JigSawFr"},
        "sameAs": [REPO_URL],
    }


def home_data(t, lang, version):
    url = f"{SITE_URL}{lang_dir(lang)}"
    html_lang = next(l[3] for l in LANGUAGES if l[0] == lang)
    return [
        {"@context": "https://schema.org", "@type": "WebSite", "name": "PlayShelf", "url": url,
         "inLanguage": html_lang},
        software(t, lang, version, url),
        {"@context": "https://schema.org", "@type": "FAQPage", "inLanguage": html_lang,
         "mainEntity": [{"@type": "Question", "name": t(f"faq_{q}_q", escape=False),
                         "acceptedAnswer": {"@type": "Answer", "text": t(f"faq_{q}_a", escape=False)}}
                        for q in FAQ]},
    ]


def doc_data(t, lang, page_name, title, description, translated):
    home = f"{SITE_URL}{lang_dir(lang)}"
    url = f"{home}docs/{page_name}/"
    content_lang = next(l[3] for l in LANGUAGES if l[0] == (lang if translated else "en-US"))
    return [
        {"@context": "https://schema.org", "@type": "TechArticle", "headline": title, "description": description,
         "url": url, "inLanguage": content_lang, "about": {"@type": "SoftwareApplication", "name": "PlayShelf"},
         "isPartOf": {"@type": "WebSite", "name": "PlayShelf", "url": home}},
        {"@context": "https://schema.org", "@type": "BreadcrumbList", "itemListElement": [
            {"@type": "ListItem", "position": 1, "name": "PlayShelf", "item": home},
            {"@type": "ListItem", "position": 2, "name": t("docs_title", escape=False), "item": f"{home}docs/getting-started/"},
            {"@type": "ListItem", "position": 3, "name": title, "item": url}]},
    ]


def llms_txt(t, version, descriptions, full_docs=None):
    """llms.txt (llmstxt.org): what PlayShelf is and where its documentation
    is, in Markdown, for AI assistants; with `full_docs`, llms-full.txt, every
    page of the documentation in it."""
    p = lambda key: t(key, escape=False)
    lines = [
        "# PlayShelf", "",
        f"> {p('meta_description')}", "",
        p("hero_text"), "",
        f"- {p('own_title')} {p('own_text')}",
        f"- {p('faq_cfw_a')}",
        (f"- Latest release: {version} — {REPO_URL}/releases/latest (`playshelf.zip`, to extract at the root of the SD card)."
         if changelog_releases() else f"- No release yet: {REPO_URL}/releases"),
        f"- Free software under the GNU GPL v3: {REPO_URL}",
        f"- Try it in a browser, against a simulated console and a demo library server: {SITE_URL}simulator/",
        (f"- The website in other languages: "
         + ", ".join(f"{SITE_URL}{lang_dir(code)}llms.txt" for code in markdown_langs() if code != "en-US"))
        if t.lang == "en-US" else f"- English: {SITE_URL}llms.txt",
        "",
        "## Documentation", "",
    ]
    lines += [f"- [{p(f'doc_{name}')}]({SITE_URL}{lang_dir(t.lang)}docs/{name}.md): {descriptions[name]}"
              for name, _ in DOCS]
    lines += ["", f"## {p('faq_title')}", ""]
    for q in FAQ:
        lines += [f"### {p(f'faq_{q}_q')}", "", p(f"faq_{q}_a"), ""]
    lines += ["## Optional", "",
              f"- [Changelog]({SITE_URL}changelog/): every release's changes (Atom feed: {SITE_URL}releases.atom)",
              f"- [Contributing]({REPO_URL}/blob/main/CONTRIBUTING.md)"]
    if not full_docs:
        lines += [f"- [Every page of the documentation in one file]({SITE_URL}{lang_dir(t.lang)}llms-full.txt)"]
        return "\n".join(lines) + "\n"
    return "\n".join(lines) + "\n\n" + "\n\n".join(full_docs) + "\n"


# ---- The changelog --------------------------------------------------------

# A release's heading in CHANGELOG.md (release-please's): "## [1.2.0](compare link) (2026-10-18)" or "## 1.0.0 (2026-10-12)".
RELEASE = re.compile(r"^## \[?(\d+\.\d+\.\d+)\]?(?:\([^)]*\))? \((\d{4}-\d\d-\d\d)\)\s*$", re.M)


def changelog_releases():
    """[(version, date, html)] of CHANGELOG.md, the newest first."""
    with open(os.path.join(ROOT, "CHANGELOG.md"), encoding="utf-8") as f:
        text = f.read()
    heads = list(RELEASE.finditer(text))
    releases = []
    for i, m in enumerate(heads):
        end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
        # Its sections one level up: h3 under the release's h2.
        body = markdown.markdown(text[m.end():end].replace("\n### ", "\n#### ").strip(), extensions=["sane_lists"])
        releases.append((m.group(1), m.group(2), body))
    return releases


def unreleased_body(t, lang):
    """What main has that no release has yet, as its notes will say it, and
    how to try it now: a development build."""
    if not UNRELEASED:
        return ""
    sections = []
    for section, entries in UNRELEASED:
        items = "".join(
            f'<li>{f"<strong>{html.escape(scope)}:</strong> " if scope else ""}{html.escape(subject)}'
            f'{f" (<a href=\"{REPO_URL}/pull/{pr}\">#{pr}</a>)" if pr else ""}</li>'
            for scope, subject, pr in entries)
        sections.append(f"      <h3>{html.escape(section)}</h3>\n      <ul>{items}</ul>")
    # main's page: the release's may not have the section yet.
    folder = "dev/" if release_serves("troubleshooting", lang) else ""
    try_it = f'../docs/{folder}troubleshooting/{section_anchor("troubleshooting", lang, "installing-another-build")}'
    return (f'      <h2 id="dev">{t("changelog_dev")}</h2>\n'
            f'      <p>{release_text(t, "changelog_dev_intro")} <a href="{try_it}">{t("changelog_dev_try")}</a></p>\n'
            + "\n".join(sections) + "\n")


def changelog_body(t, lang):
    """The changelog page: what main has since the latest release, then each
    release, its date, its notes (in English) and its download."""
    note = "" if lang == "en-US" else f'      <p class="callout note">{t("changelog_english")}</p>\n'
    parts = [unreleased_body(t, lang)] if UNRELEASED else []
    if not changelog_releases():
        parts.append(f'      <p>{t("no_release")}</p>')
    for i, (version, date, body) in enumerate(changelog_releases()):
        notes = (f'{body.replace("<h4>", "<h3>").replace("</h4>", "</h3>")}\n'
                 f'      <p><a href="{REPO_URL}/releases/tag/v{version}">GitHub · v{version}</a></p>')
        when = f'<small><time datetime="{date}">{date}</time></small>'
        if i == 0:
            parts.append(f'      <div class="release-latest" data-version="{version}">'
                         f'<h2 id="v{version}">{version} {when}</h2>\n{notes}</div>')
            continue
        # The older ones folded, each open from a link to it (site.js).
        if i == 1:
            parts.append(f'      <h2 id="older">{t("changelog_older")}</h2>')
        parts.append(f'      <details class="release" id="v{version}" data-version="{version}"><summary>{version} {when}</summary>\n{notes}\n      </details>')
    # What changed since the version a console has (site.js): the releases
    # after it, the others hidden.
    since = (f'      <div class="since" hidden data-many="{t("since_many")}" data-latest="{t("since_latest")}"'
             f' data-unknown="{t("since_unknown")}">\n'
             f'        <label for="since-version">{t("since_label")}</label>\n'
             f'        <input id="since-version" type="text" inputmode="decimal" autocomplete="off" autocorrect="off"'
             f' spellcheck="false" enterkeyhint="done" placeholder="1.0.0">\n'
             f'        <p class="since-result" aria-live="polite"></p>\n      </div>\n')
    return f"""    <section class="section narrow">
      <article class="prose">
        <h1>{t('footer_changelog')}</h1>
{note}{since}{chr(10).join(parts)}
        <p class="edit"><a href="{SITE_URL}releases.atom">Atom</a> · <a href="{REPO_URL}/blob/main/CHANGELOG.md">CHANGELOG.md</a></p>
      </article>
    </section>
"""


def releases_atom():
    """An Atom feed of the releases: a reader, or an assistant, follows PlayShelf's new versions."""
    releases = changelog_releases()
    entries = "".join(
        f"""  <entry>
    <title>PlayShelf {version}</title>
    <id>{REPO_URL}/releases/tag/v{version}</id>
    <link rel="alternate" type="text/html" href="{SITE_URL}changelog/#v{version}"/>
    <updated>{date}T00:00:00Z</updated>
    <content type="html">{html.escape(body)}</content>
  </entry>
""" for version, date, body in releases)
    updated = releases[0][1] if releases else "2026-01-01"
    return f"""<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
  <title>PlayShelf releases</title>
  <subtitle>Your game library, on a Nintendo Switch with Atmosphère</subtitle>
  <id>{SITE_URL}releases.atom</id>
  <link rel="self" href="{SITE_URL}releases.atom"/>
  <link rel="alternate" type="text/html" href="{SITE_URL}changelog/"/>
  <updated>{updated}T00:00:00Z</updated>
  <author><name>JigSawFr</name><uri>{REPO_URL}</uri></author>
  <icon>{SITE_URL}assets/icon.svg</icon>
{entries}</feed>
"""


def sitemap(paths):
    """Every page in every language, each with its other languages."""
    entries = []
    for path in paths:
        alternates = "".join(
            f'\n    <xhtml:link rel="alternate" hreflang="{hl}" href="{SITE_URL}{f"{d}/" if d else ""}{path}"/>'
            for _, d, _, hl in LANGUAGES)
        alternates += f'\n    <xhtml:link rel="alternate" hreflang="x-default" href="{SITE_URL}{path}"/>'
        for _, d, _, _ in LANGUAGES:
            entries.append(f'  <url>\n    <loc>{SITE_URL}{f"{d}/" if d else ""}{path}</loc>{alternates}\n  </url>')
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" xmlns:xhtml="http://www.w3.org/1999/xhtml">\n'
            + "\n".join(entries) + "\n</urlset>\n")


def redirect_page(to, canonical, html_lang):
    """A page that sends its visitor on at once (GitHub Pages has no
    redirects of its own): no script, which the CSP refuses inline."""
    return (f'<!doctype html>\n<html lang="{html_lang}">\n<meta charset="utf-8">\n'
            f'<meta name="robots" content="noindex">\n<meta http-equiv="refresh" content="0; url={to}">\n'
            f'<link rel="canonical" href="{canonical}">\n<title>PlayShelf</title>\n<a href="{to}">{canonical}</a>\n')


def favicon_ico(png):
    """An ICO holding one PNG, which every browser reads: what a domain's
    root is asked for without the page's <link rel="icon">."""
    width, height = struct.unpack(">II", png[16:24])   # the PNG's IHDR
    return (struct.pack("<HHH", 0, 1, 1)
            + struct.pack("<BBBBHHII", width % 256, height % 256, 0, 0, 1, 32, len(png), 22) + png)


def write(out, rel, text):
    path = os.path.join(out, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def main():
    global ROOT
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    out = sys.argv[1]
    ROOT = sys.argv[2] if len(sys.argv) > 2 else "."
    # The languages site/i18n/ has a catalog for, English first.
    global LANGUAGES
    LANGUAGES = [l for l in LANGUAGES if os.path.exists(os.path.join(ROOT, "site", "i18n", f"{l[0]}.json"))]
    version = "v" + read_json(".release-please-manifest.json")["."]
    # The documentation in two versions: main's, and the latest release's
    # out of its tag, with what main has since (git's history: none in a
    # tarball or a shallow clone, and the site has main's alone).
    global DOCSET, DEV_DOCS, RELEASE_DOCS, LATEST_TAG, UNRELEASED
    DEV_DOCS = DOCSET = DocSet(ROOT, list(DOCS))
    LATEST_TAG = latest_tag()
    release_dir = tempfile.mkdtemp(prefix="playshelf-release-")
    if LATEST_TAG:
        RELEASE_DOCS = release_docs(LATEST_TAG, release_dir)
        UNRELEASED = unreleased(LATEST_TAG)
    if RELEASE_DOCS:
        compare_release()
    os.makedirs(out, exist_ok=True)
    assets = {"images/social-preview.png"}
    pages = 0
    md_langs = markdown_langs()
    for lang, folder, _, html_lang in LANGUAGES:
        t = Texts(lang, version)
        base = f"{folder}/" if folder else ""
        to_root = "../" if folder else ""
        assets.update(screenshot(lang, s) for s in SCREENSHOTS)
        assets.add(social_image(lang))
        assets.update(filter(None, (screenshot(lang, s, dark=True) for s in SCREENSHOTS)))
        write(out, f"{base}index.html", page(t, lang, to_root, "", "", home_body(t, lang, to_root), body_class="home",
                                             full_title=f"PlayShelf — {t('hero_title', escape=False)}",
                                             structured=home_data(t, lang, version)))
        descriptions, markdown, index = {}, [], []
        for name, _ in DOCS:
            doc_root = to_root + "../../"
            # The site's page: the latest release's where it has it in this
            # language (what a console runs), else main's.
            source = RELEASE_DOCS if release_serves(name, lang) else DEV_DOCS
            DOCSET = DocSet(source.root, list(DOCS), source.tag, "docs/")
            title, translated, description, content, body = doc_body(t, lang, doc_root, name, assets)
            DOCSET = DEV_DOCS
            index += search_entries(name, t(f"doc_{name}", escape=False), content)
            descriptions[name] = description
            md_lang = markdown_lang(name, lang)
            # A page not translated yet is named in the reader's language (its sidebar's name).
            shown = title if translated else t(f"doc_{name}", escape=False)
            write(out, f"{base}docs/{name}/index.html",
                  page(t, lang, doc_root, f"docs/{name}/", shown, body, description=html.escape(description),
                       structured=doc_data(t, lang, name, shown, description, translated),
                       markdown_url=f"{SITE_URL}{lang_dir(md_lang)}docs/{name}.md"))
            # The Markdown of each page, in the languages it is written in.
            if lang in md_langs:
                text = markdown_doc(name, lang, assets)
                write(out, f"{base}docs/{name}.md", text)
                markdown.append(text)
            pages += 1
        # docs/ itself (an address typed or cut short by hand): the guide's
        # first page, not the page not found.
        first = DOCS[0][0]
        write(out, f"{base}docs/index.html", redirect_page(f"{first}/", f"{SITE_URL}{base}docs/{first}/", html_lang))
        index += other_entries(t)
        write(out, f"{base}docs/search.json", json.dumps(index, ensure_ascii=False, separators=(",", ":")))
        write(out, f"{base}changelog/index.html",
              page(t, lang, to_root + "../", "changelog/", t("footer_changelog", escape=False), changelog_body(t, lang)))
        pages += 1
        # Each version at its own address, the one docs/ does not show beside
        # it: main's (docs/dev/) and the latest release's (docs/<tag>/, its
        # pages in its own languages, English otherwise, said so). Not
        # indexed: docs/ is the site's.
        if RELEASE_DOCS:
            for docset, label in ((DocSet(DEV_DOCS.root, list(DOCS), None, "docs/dev/"), t("doc_dev", escape=False)),
                                  (RELEASE_DOCS, RELEASE_DOCS.tag)):
                DOCSET = docset
                for name, _ in docset.pages:
                    title, translated, description, _, body = doc_body(t, lang, to_root + "../../../", name, assets)
                    shown = title if translated else t(f"doc_{name}", escape=False)
                    write(out, f"{base}{docset.prefix}{name}/index.html",
                          page(t, lang, to_root + "../../../", f"{docset.prefix}{name}/", f"{shown} · {label}",
                               body, description=html.escape(description), indexed=False))
                    pages += 1
            DOCSET = DEV_DOCS
        if lang in md_langs:
            write(out, f"{base}llms.txt", llms_txt(t, version, descriptions))
            write(out, f"{base}llms-full.txt", llms_txt(t, version, descriptions, markdown))
        pages += 1
    write(out, "sitemap.xml", sitemap([""] + [f"docs/{name}/" for name, _ in DOCS] + ["changelog/"]))
    write(out, "releases.atom", releases_atom())
    # robots.txt counts only at a domain's root: on a custom domain (not
    # <owner>.github.io/<repo>/, nor a preview's folder). Every crawler,
    # search engines' and AI assistants' alike, is welcome.
    if urlsplit(SITE_URL).path == "/":
        write(out, "robots.txt", f"User-agent: *\nAllow: /\n\nSitemap: {SITE_URL}sitemap.xml\n")
    # GitHub Pages serves 404.html at any path: its links start from the site's root.
    t = Texts("en-US", version)
    base_url = os.environ.get("SITE_BASE", "/PlayShelf/")
    # Its text in every language, the English one shown: site.js shows the
    # one of the address' folder (/fr/…), else the visitor's.
    blocks = []
    for lang, folder, _, html_lang in LANGUAGES:
        tl = Texts(lang, version)
        blocks.append(f'      <div class="not-found" data-lang="{lang}" lang="{html_lang}"{" hidden" if lang != "en-US" else ""}>'
                      f'<h1>{tl("not_found_title")}</h1><p>{tl("not_found_text")}</p>'
                      f'<p><a class="button primary" href="{base_url}{f"{folder}/" if folder else ""}">{tl("home")}</a></p></div>')
    write(out, "404.html", page(t, "en-US", base_url, "", t("not_found_title"),
                                '    <section class="section narrow">\n' + "\n".join(blocks) + "\n    </section>",
                                body_class="not-found-page", indexed=False))
    # The site's own files, then what the pages use from the repository and
    # the screenshots of tools/site_screenshots.mjs.
    os.makedirs(os.path.join(out, "assets"), exist_ok=True)
    shutil.copytree(os.path.join(ROOT, "site", "assets"), os.path.join(out, "assets"), dirs_exist_ok=True)
    for name in ("icon.svg", "mark.svg"):
        shutil.copy(os.path.join(ROOT, "branding", name), os.path.join(out, "assets", name))
    # An iPhone's home screen takes a PNG, not the SVG (tools/render_branding.mjs).
    touch = os.path.join(ROOT, "images", "apple-touch-icon.png")
    shutil.copy(touch, os.path.join(out, "assets", "apple-touch-icon.png"))
    # Added to a phone's home screen: "PlayShelf", not the page's whole title
    # (Android reads the manifest, iOS apple-mobile-web-app-title). The site
    # opens in the browser, at its root (the visitor's language from there).
    write(out, "assets/site.webmanifest", json.dumps({
        "name": "PlayShelf", "short_name": "PlayShelf", "start_url": "../", "scope": "../", "display": "browser",
        "background_color": "#ffffff", "theme_color": "#0a705f",
        "icons": [{"src": "icon.svg", "sizes": "any", "type": "image/svg+xml"},
                  {"src": "icon-192.png", "sizes": "192x192", "type": "image/png"},
                  {"src": "icon-512.png", "sizes": "512x512", "type": "image/png"},
                  {"src": "icon-512.png", "sizes": "512x512", "type": "image/png", "purpose": "maskable"}],
        # A long press on the icon (Android): the simulator, in the phone's
        # language (one manifest for every language: a page of the
        # documentation would be the English one).
        "shortcuts": [{"name": "Simulator", "url": "../simulator/",
                       "icons": [{"src": "icon-192.png", "sizes": "192x192", "type": "image/png"}]}]}, indent=1) + "\n")
    for name in ("icon-192.png", "icon-512.png"):
        shutil.copy(os.path.join(ROOT, "images", name), os.path.join(out, "assets", name))
    # At a domain's root (as robots.txt), what browsers, bookmarks, iOS and
    # link previews ask for there without reading the page.
    if urlsplit(SITE_URL).path == "/":
        shutil.copy(touch, os.path.join(out, "apple-touch-icon.png"))
        with open(touch, "rb") as f, open(os.path.join(out, "favicon.ico"), "wb") as ico:
            ico.write(favicon_ico(f.read()))
    for path in sorted(assets):
        os.makedirs(os.path.dirname(os.path.join(out, "assets", path)), exist_ok=True)
        if path in RELEASE_FILES:
            source = RELEASE_FILES[path]
        elif path == "images/social-preview.png":
            source = os.path.join(ROOT, ".github", "readme", "social-preview.png")
        elif path.startswith("images/screenshots/") and path.count("/") == 3:
            source = os.path.join(SCREENSHOT_DIR, *path.split("/")[2:])
        else:
            source = os.path.join(ROOT, path)
        shutil.copy(source, os.path.join(out, "assets", path))
    shutil.rmtree(release_dir, ignore_errors=True)
    open(os.path.join(out, ".nojekyll"), "w").close()
    print(f"site: {pages} pages in {len(LANGUAGES)} languages, {len(assets)} images, in {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
