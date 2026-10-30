#!/usr/bin/env python3
"""Checks the links of the built website (site/README.md): every link and
image of its pages that stays on the site must reach a file of it, and every
#anchor an id of the page it points to. Links to the simulator (built apart,
web/README.md) and to other sites are not followed.

Links out follow the rule "liens sortants anonymes": every page sends no
referrer (<meta name="referrer" content="no-referrer">), a link to a third
party has rel="noreferrer", one to boreale.dev, *.boreale.dev or a repository
of github.com/JigSawFr/ has referrerpolicy="strict-origin" (the origin alone,
for GitHub's traffic insights) and no noreferrer.

Usage: tools/check_site_links.py <site-dir> [site-url]
(site-url: the site's own address, whose absolute links are checked too)
"""
import os
import sys
from html.parser import HTMLParser
from urllib.parse import unquote, urlparse, urlsplit



def own_site(url: str) -> bool:
    """boreale.dev, its subdomains, and the repositories of github.com/JigSawFr/
    (that account's alone: another owner, a gist or GitHub's own pages are
    third parties)."""
    u = urlparse(url)
    host = (u.hostname or "").lower()
    return u.scheme == "https" and (host == "boreale.dev" or host.endswith(".boreale.dev") or
                                    (host == "github.com" and u.path.lower().startswith("/jigsawfr/")))


def check_own_site() -> None:
    """own_site() against look-alikes, at each run (the site has no unit tests)."""
    for url in ("https://boreale.dev/", "https://shelf.boreale.dev/fr/", "https://github.com/JigSawFr/PlayShelf",
                "https://github.com/jigsawfr/boreale/releases"):
        assert own_site(url), url
    for url in ("https://store.example.com/app/1", "https://github.com/JigSawFr", "https://github.com/jigsawfrx/x",
                "https://github.com/someone/JigSawFr", "https://gist.github.com/JigSawFr/1",
                "https://boreale.dev.example.com/", "https://notboreale.dev/", "http://boreale.dev/"):
        assert not own_site(url), url


def link_problem(attrs: dict) -> str:
    """What a link out does wrong by the rule "liens sortants anonymes": a
    third party gets rel="noreferrer"; *.boreale.dev and github.com/JigSawFr
    keep the origin alone (referrerpolicy="strict-origin", no noreferrer)."""
    href = attrs.get("href") or ""
    if urlparse(href).scheme not in ("http", "https"):
        return ""
    rel = (attrs.get("rel") or "").split()
    if own_site(href):
        if "noreferrer" in rel or attrs.get("referrerpolicy") != "strict-origin":
            return f'link to {href} needs referrerpolicy="strict-origin" and no noreferrer'
    elif "noreferrer" not in rel:
        return f'link to {href} without rel="noreferrer"'
    return ""


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links, self.ids, self.problems = [], set(), []
        self.no_referrer = False

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if "id" in a:
            self.ids.add(a["id"])
        if tag == "meta" and a.get("name") == "referrer" and a.get("content") == "no-referrer":
            self.no_referrer = True
        if tag == "a" and link_problem(a):
            self.problems.append(link_problem(a))
        for key in ("href", "src", "poster"):
            # <link rel="alternate|canonical|sitemap"> name pages by address: checked as links too.
            if a.get(key):
                self.links.append(a[key])


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    root = os.path.abspath(sys.argv[1])
    site = sys.argv[2].rstrip("/") + "/" if len(sys.argv) > 2 else None
    pages = {}
    for folder, _, files in os.walk(root):
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

    errors = 0
    check_own_site()
    for path, page in sorted(pages.items()):
        for problem in page.problems + ([] if page.no_referrer else ['no <meta name="referrer" content="no-referrer">']):
            print(f"{os.path.relpath(path, root)}: {problem}")
            errors += 1
        if os.path.basename(path) == "404.html":
            continue   # served at any path: its links start from the site's root, elsewhere
        for link in page.links:
            if site and link.startswith(site):
                link = os.path.relpath(os.path.join(root, link[len(site):]), os.path.dirname(path)) + \
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
                print(f"{os.path.relpath(path, root)}: {link}: no {rel}")
                errors += 1
            elif parts.fragment and file.endswith(".html"):
                ids = pages[file].ids if file in pages else set()
                if unquote(parts.fragment) not in ids:
                    print(f"{os.path.relpath(path, root)}: {link}: no #{unquote(parts.fragment)} in {rel}")
                    errors += 1
    print(f"site links: {len(pages)} pages, {errors} problem(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
