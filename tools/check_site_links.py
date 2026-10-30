#!/usr/bin/env python3
"""Checks the built website (site/README.md).

boreale-site's checks, those every one of Boréale's sites passes
(boreale_site.check.pages()): every link and image of its pages that stays
on the site reaches a file of it, and every #anchor an id of the page; links
out by the rule "liens sortants anonymes" (no referrer sent, rel="noreferrer"
to a third party, referrerpolicy="strict-origin" to *.boreale.dev and
github.com/JigSawFr) and their UTM tags (utm_source=shelf and a utm_medium to
another of our sites, none to anyone else); the Content-Security-Policy, no
inline script or style; Umami's tracker; no real e-mail address. And its
catalogs (boreale_site.check_catalogs()): site/i18n/ and boreale-site's i18n/,
none repeating a common text.

Left out of them, as PlayShelf's site is made:
- the links to the simulator (simulator/), built apart (web/README.md);
- 404.html's links: served at any address, they start from the site's path
  (SITE_BASE), not from where the file is;
- a page that sends its visitor on at once (docs/: a <meta> refresh): no
  policy, no tracker, nothing of it is seen;
- the tracker of a pull request's preview (SITE_NOINDEX=1), which has none.

PlayShelf's own: the links to the site's own address (site-url: the canonical
page, the other languages), as the files they lead to.

Usage: tools/check_site_links.py <site-dir> [site-url]
(site-url: the site's own address, whose absolute links are checked too)
"""
import os
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "library" / "boreale-site"))
import boreale_site as boreale  # noqa: E402
from boreale_site import check  # noqa: E402

# A site's catalog may leave a text out: the English one is shown
# (build_site.py's Texts). None does today.
OPTIONAL = {}


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links, self.ids = [], set()
        self.redirect = False

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if "id" in a:
            self.ids.add(a["id"])
        if tag == "meta" and (a.get("http-equiv") or "").lower() == "refresh":
            self.redirect = True
        for key in ("href", "src"):
            # <link rel="alternate|canonical|sitemap"> name pages by address: checked as links too.
            if a.get(key):
                self.links.append(a[key])


def common_errors(root: str, site: boreale.Site, pages: dict) -> list:
    """boreale-site's checks, but for what PlayShelf's site does otherwise
    (the docstring)."""
    redirects = {os.path.relpath(p, root).replace(os.sep, "/") for p, page in pages.items() if page.redirect}
    preview = os.environ.get("SITE_NOINDEX") == "1"
    errors, _ = check.pages(Path(root), site, skip=("assets", "simulator"))
    kept = []
    for error in errors:
        name, _, problem = error.partition(": ")
        if problem.endswith(" leads nowhere") and (name == "404.html" or "simulator/" in problem):
            continue
        if name in redirects and problem in ("no Content-Security-Policy", "no Umami tracker (assets/umami.js)"):
            continue
        if preview and problem == "no Umami tracker (assets/umami.js)":
            continue
        kept.append(error)
    return kept


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    root = os.path.abspath(sys.argv[1])
    site_url = sys.argv[2].rstrip("/") + "/" if len(sys.argv) > 2 else None
    site = boreale.Site("playshelf", site_url or os.environ.get("SITE_URL"))
    pages = {}
    for folder, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if os.path.relpath(os.path.join(folder, d), root) != "simulator"]
        for name in files:
            if name.endswith(".html"):
                path = os.path.join(folder, name)
                page = Page()
                with open(path, encoding="utf-8") as f:
                    page.feed(f.read())
                pages[path] = page

    def target(path):
        if os.path.isdir(path):
            path = os.path.join(path, "index.html")
        return path

    problems = common_errors(root, site, pages)
    problems += boreale.check_catalogs(ROOT / "site" / "i18n", OPTIONAL)
    for path, page in sorted(pages.items()):
        if os.path.basename(path) == "404.html":
            continue   # served at any path: its links start from the site's root, elsewhere
        for link in page.links:
            # The site's own address (absolute: canonical, other languages):
            # boreale-site's checks follow the others.
            if not site_url or not link.startswith(site_url):
                continue
            link = os.path.relpath(os.path.join(root, link[len(site_url):]), os.path.dirname(path)) + \
                ("/" if link.endswith("/") else "")
            parts = urlsplit(link)
            if parts.scheme or link.startswith(("mailto:", "data:", "//")):
                continue
            file = target(os.path.normpath(os.path.join(os.path.dirname(path), unquote(parts.path)))) \
                if parts.path else path
            rel = os.path.relpath(file, root)
            if rel.split(os.sep)[0] == "simulator" or rel.startswith(".."):
                continue
            if not os.path.exists(file):
                problems.append(f"{os.path.relpath(path, root)}: {link}: no {rel}")
            elif parts.fragment and file.endswith(".html"):
                ids = pages[file].ids if file in pages else set()
                if unquote(parts.fragment) not in ids:
                    problems.append(f"{os.path.relpath(path, root)}: {link}: no #{unquote(parts.fragment)} in {rel}")
    for problem in problems:
        print(problem)
    print(f"site links: {len(pages)} pages, {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
