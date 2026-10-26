#!/usr/bin/env python3
"""Checks the links of the built website (site/README.md): every link and
image of its pages that stays on the site must reach a file of it, and every
#anchor an id of the page it points to. Links to the simulator (built apart,
web/README.md) and to other sites are not followed.

Usage: tools/check_site_links.py <site-dir> [site-url]
(site-url: the site's own address, whose absolute links are checked too)
"""
import os
import sys
from html.parser import HTMLParser
from urllib.parse import unquote, urlsplit


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links, self.ids = [], set()

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if "id" in a:
            self.ids.add(a["id"])
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
    for path, page in sorted(pages.items()):
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
    print(f"site links: {len(pages)} pages, {errors} broken link(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
