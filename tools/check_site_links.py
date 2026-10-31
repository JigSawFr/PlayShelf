#!/usr/bin/env python3
"""Checks the built website (site/README.md): boreale-site's checks, those
every one of Boréale's sites passes, as this site is made.

boreale_site.check.pages(): every link and image of its pages that stays on
the site reaches a file of it, the site's own address included (site-url:
the canonical page, the other languages, the sitemap), and every #anchor an
id of the page; links out by the rule "liens sortants anonymes" (no referrer
sent, rel="noreferrer" to a third party, referrerpolicy="strict-origin" to
*.boreale.dev and github.com/JigSawFr) and their UTM tags (utm_source=shelf
and a utm_medium to another of our sites, none to anyone else); the
Content-Security-Policy, no inline script or style; Umami's tracker; no real
e-mail address. With, for this site:
- the simulator (simulator/), built apart (web/README.md): not walked, the
  links into it not followed;
- 404.html, served at any address: its links start from SITE_BASE (a pull
  request's preview, /PlayShelf/pr-<n>/), not from where the file is;
- a preview (SITE_NOINDEX=1) counts no one: no tracker.
- the page chrome (library/boreale-site, "The page chrome (v4)"): one header
  with the name's menu and the nav, the call to action (the simulator) before
  GitHub, the footer linking to every other of Boréale's sites, chrome.css and
  chrome.js loaded.
docs/'s pages that send their visitor on (a <meta> refresh) need neither
policy nor tracker there.

boreale_site.check_catalogs(): site/i18n/ and boreale-site's i18n/, none
repeating a common text.

Usage: tools/check_site_links.py <site-dir> [site-url]
(site-url: the site's own address, whose absolute links are checked too;
SITE_URL's by default)
"""
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "library" / "boreale-site"))
import boreale_site as boreale  # noqa: E402
from boreale_site import check  # noqa: E402

# A site's catalog may leave a text out: the English one is shown
# (build_site.py's Texts). None does today.
OPTIONAL = {}


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    out = Path(sys.argv[1])
    site_url = sys.argv[2].rstrip("/") + "/" if len(sys.argv) > 2 else None
    site = boreale.Site("playshelf", site_url or os.environ.get("SITE_URL"))
    problems, count = check.pages(out, site, apart=("simulator",), tracker=os.environ.get("SITE_NOINDEX") != "1",
                                  base=os.environ.get("SITE_BASE"), chrome=True)
    problems += boreale.check_catalogs(ROOT / "site" / "i18n", OPTIONAL)
    for problem in problems:
        print(problem)
    print(f"site links: {count} pages, {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
