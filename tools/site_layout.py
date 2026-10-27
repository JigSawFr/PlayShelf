#!/usr/bin/env python3
"""The website's layout on a phone, a tablet and a desktop (site/README.md).

Serves a built site (tools/build_site.py <out-dir>) and opens its pages in
headless Chromium (Playwright), failing when one:

- is wider than the screen (a long word, a table, a header that does not
  fit): the page scrolls sideways;
- throws (site.js);
- has a field under 16px on a phone (iOS zooms into it when it is touched).

Every page on an iPhone (402x874); the pages most likely to be too wide (the
home page, the documentation's widest, the changelog) in each language also
on a small phone (320), a phone in landscape (874x402), a tablet (1024) and
a desktop (1440). The images are not loaded: their boxes are
in the pages (width, height), and the check takes seconds, not minutes.

With SITE_AXE set to axe-core's axe.min.js, also what axe finds against WCAG
2.2 A and AA (contrast, names, labels, landmarks, keyboard), on an iPhone:
every English page in both themes, and each other language's home page (its
texts, not its layout nor its colours, differ). Every page in every language
would take a minute more; this, about fifteen seconds.

Usage: tools/site_layout.py <site-dir>   (needs Playwright for Python with
Chromium, PLAYWRIGHT_CHROMIUM=<path> for a browser of its own; SITE_LAYOUT_JOBS
pages at once, 6 by default)
"""
import asyncio
import functools
import re
import http.server
import os
import sys
import threading

from playwright.async_api import async_playwright

SITE = sys.argv[1]
JOBS = int(os.environ.get("SITE_LAYOUT_JOBS", "6"))
AXE = os.environ.get("SITE_AXE")
AXE_TAGS = ["wcag2a", "wcag2aa", "wcag21a", "wcag21aa", "wcag22aa"]

# (name, width, height, a phone): the screens.
PHONE = ("iPhone", 402, 874, True)
OTHERS = [("small phone", 320, 640, True), ("phone in landscape", 874, 402, True),
          ("tablet", 1024, 768, False), ("desktop", 1440, 900, False)]
# The pages every screen sees, under each language's folder.
WIDEST = ["", "docs/config/", "docs/servers/", "docs/downloads/", "changelog/"]

# What a page reports: how wide it is, what is wider than the screen (its
# width: on a phone, the page's viewport widens to the page), the fields
# under 16px.
MEASURE = """(screen) => {
  const out = { width: document.documentElement.scrollWidth, wide: [], small: [] };
  if (out.width > screen + 1) {
    // The widest elements not inside something that scrolls on its own.
    for (const e of document.querySelectorAll('body *')) {
      if (e.getBoundingClientRect().right <= screen + 1) continue;
      let a = e.parentElement, clipped = false;
      for (; a && a !== document.body; a = a.parentElement) {
        if (getComputedStyle(a).overflowX !== 'visible') { clipped = true; break; }
      }
      if (!clipped) out.wide.push(`${e.tagName.toLowerCase()}${e.className ? '.' + String(e.className).trim().split(/\\s+/).join('.') : ''}`
                                  + ` "${(e.textContent || '').trim().slice(0, 40)}"`);
      if (out.wide.length >= 3) break;
    }
  }
  for (const e of document.querySelectorAll('input, select, textarea')) {
    if (e.type === 'checkbox' || e.type === 'radio' || e.type === 'hidden') continue;
    if (parseFloat(getComputedStyle(e).fontSize) < 16) out.small.push(e.id || e.name || e.tagName.toLowerCase());
  }
  return out;
}"""


def pages():
    """Each language's folder ('' for English) and its pages, from the built site."""
    folders = [""] + sorted(d + "/" for d in os.listdir(SITE)
                            if os.path.isfile(os.path.join(SITE, d, "index.html")) and d not in ("simulator", "changelog", "docs"))
    every = []
    for root, dirs, files in os.walk(SITE):
        dirs[:] = [d for d in dirs if d not in ("simulator", "assets")]
        # Not a page that sends its visitor on (docs/: the guide's first page).
        if "index.html" in files and 'http-equiv="refresh"' not in open(os.path.join(root, "index.html"), encoding="utf-8").read():
            every.append(os.path.relpath(root, SITE).replace(os.sep, "/").removeprefix(".") + "/")
    # Each version at its own address (docs/dev/, docs/v<x>/) in English and
    # French only: in the other languages, the same pages as docs/ or the
    # same English page.
    every = sorted(p.lstrip("/") for p in every
                   if not re.search(r"(^|/)docs/(dev|v\d[^/]*)/", p.lstrip("/")) or p.lstrip("/").split("/")[0] in ("docs", "fr"))
    widest = [f + p for f in folders for p in WIDEST if os.path.isfile(os.path.join(SITE, f + p, "index.html"))]
    english = [p for p in every if p.split("/", 1)[0] + "/" not in folders[1:]]
    return every, widest, english


class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def serve():
    handler = functools.partial(Quiet, directory=SITE)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return f"http://127.0.0.1:{server.server_address[1]}/"


async def check(browser, base, screen, paths, failures):
    name, width, height, phone = screen
    context = await browser.new_context(viewport={"width": width, "height": height},
                                        is_mobile=phone, has_touch=phone)
    # No images nor video: their boxes are in the pages.
    await context.route("**/*.{png,webp,jpg,jpeg,gif,mp4}", lambda route: route.abort())
    queue = list(paths)

    async def worker():
        page = await context.new_page()
        errors = []
        page.on("pageerror", lambda e: errors.append(str(e)))
        while queue:
            path = queue.pop()
            errors.clear()
            await page.goto(base + path, wait_until="load")
            m = await page.evaluate(MEASURE, width)
            where = f"{name} ({width}px) /{path}"
            if m["width"] > width + 1:
                failures.append(f"{where}: {m['width']}px wide: " + ", ".join(m["wide"]))
            if phone and m["small"]:
                failures.append(f"{where}: fields under 16px (iOS zooms into them): {', '.join(m['small'])}")
            for e in errors:
                failures.append(f"{where}: {e}")

    await asyncio.gather(*(worker() for _ in range(min(JOBS, len(paths)))))
    await context.close()
    return len(paths)


async def accessible(browser, base, theme, paths, failures):
    """axe's violations on each page, in the light or the dark theme."""
    name, width, height, phone = PHONE
    # axe is a script added to the page, which the site's CSP refuses.
    context = await browser.new_context(viewport={"width": width, "height": height}, is_mobile=phone,
                                        has_touch=phone, color_scheme=theme, bypass_csp=True)
    await context.route("**/*.{png,webp,jpg,jpeg,gif,mp4}", lambda route: route.abort())
    source = open(AXE, encoding="utf-8").read()
    queue = list(paths)

    async def worker():
        page = await context.new_page()
        while queue:
            path = queue.pop()
            await page.goto(base + path, wait_until="load")
            await page.add_script_tag(content=source)
            found = await page.evaluate("""(tags) => axe.run(document, {
                runOnly: { type: 'tag', values: tags }, resultTypes: ['violations'] })
              .then((r) => r.violations.map((v) => ({ id: v.id, help: v.help,
                where: v.nodes.slice(0, 3).map((n) => n.target.join(' ')) })))""", AXE_TAGS)
            for v in found:
                failures.append(f"axe, {theme} theme, /{path}: {v['id']} ({v['help']}): {', '.join(v['where'])}")

    await asyncio.gather(*(worker() for _ in range(min(JOBS, len(paths)))))
    await context.close()
    return len(paths)


async def main():
    every, widest, english = pages()
    base = serve()
    failures = []
    async with async_playwright() as p:
        browser = await p.chromium.launch(executable_path=os.environ.get("PLAYWRIGHT_CHROMIUM") or None)
        count = await check(browser, base, PHONE, every, failures)
        for screen in OTHERS:
            count += await check(browser, base, screen, widest, failures)
        if AXE:
            others = [f for f in widest if f.count("/") == 1 and f not in english]   # "fr/": a home page
            checked = await accessible(browser, base, "light", english + others, failures)
            checked += await accessible(browser, base, "dark", english, failures)
        await browser.close()
    for f in failures:
        print(f"::error::{f}" if os.environ.get("GITHUB_ACTIONS") else f)
    print(f"site layout: {count} pages checked ({len(every)} on an iPhone, {len(widest)} on each of {len(OTHERS)} other screens)"
          + (f", {checked} with axe" if AXE else "") + f", {len(failures)} problem(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
