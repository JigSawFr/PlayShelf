#!/usr/bin/env python3
"""The website's screenshots (site/README.md): the screens of its gallery
and documentation, taken from the web simulator in headless Chromium in each
language of the site and in both themes, as
<out-dir>/<language>/<screen>.webp and <screen>-dark.webp (1280x720, the
canvas). tools/build_site.py uses them when SITE_SCREENSHOTS names
<out-dir>; without them, the site has the English ones of
images/screenshots/, which this script also makes (--png: the English light
ones as images/screenshots/<screen>.png).

The console is the smoke test's (tools/web_smoke.py): the same moment, the
focus still, the demo library server; a game of it installed on the way, so
the Library and the Downloads show it.

Usage: tools/site_screenshots.py <simulator-dir> <out-dir> [language…] [--png]
(the site's languages, site/i18n/*.json, by default; needs Playwright for
Python with Chromium; PLAYWRIGHT_CHROMIUM=<path> for a browser of its own)
"""
import base64
import functools
import glob
import http.server
import os
import sys
import threading
import time

from playwright.sync_api import sync_playwright

# The gallery's screens, in the order they are taken: each from the one before.
SCREENS = ["catalogue", "game_sheet", "downloads", "library", "preferences"]
FIXED = "now=1791471600&still_focus=1&version=1.0.0:1a2b3c4&tab=catalogue&speed=0"
START_S = 120


class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def log_message(self, *args):
        pass


def settle(page, seconds=1.5):
    """A moment for the screen to settle (borealis' animations, a toast), and
    as long again as borealis still holds the input, up to 5 s."""
    time.sleep(seconds)
    end = time.time() + 5
    while time.time() < end and page.evaluate("Module.ccall('playshelf_web_busy', 'number', [], [])"):
        time.sleep(0.1)
    page.evaluate("new Promise((done) => requestAnimationFrame(() => requestAnimationFrame(done)))")


def logged(page, text, seconds):
    end = time.time() + seconds
    while time.time() < end:
        if page.evaluate("(t) => logLines.some(([line]) => line.includes(t))", text):
            return True
        time.sleep(0.25)
    return False


def take(browser, base, lang, theme, out):
    """The screens in `lang` and `theme`: True when they were all taken."""
    context = browser.new_context(viewport={"width": 1600, "height": 1000}, locale=lang, bypass_csp=True)
    page = context.new_page()
    try:
        page.goto(f"{base}?{FIXED}&lang={lang}&theme={theme}")
        page.wait_for_function("document.body && document.body.dataset.playshelf === 'ready'", timeout=START_S * 1000)
        shots = {}

        def shot(name, seconds=1.5):
            settle(page, seconds)
            data = page.evaluate("canvas.toDataURL('image/webp', 0.9)")
            shots[name] = base64.b64decode(data.split(",", 1)[1])

        time.sleep(2)   # the catalogue and its icons, from the demo server
        shot("catalogue")
        page.evaluate("(s) => runSteps(s)", "right.down*4.a")
        shot("game_sheet")
        page.evaluate("(s) => runSteps(s)", "a")
        # Its packages are named in the page's language: their version alone is waited for.
        if not logged(page, "v65536 installed", 60):
            print(f"site screenshots: {lang} {theme}: the demo game was not installed")
            return False
        # Its packages are named in the language: only its version is waited for.
        # Its toast gone (3 s), then the tabs from the Downloads up to the Library.
        page.evaluate("(s) => runSteps(s)", "b.left.down")
        shot("downloads", 5)
        page.evaluate("(s) => runSteps(s)", "up*2")
        shot("library")
        page.evaluate("(s) => runSteps(s)", "down*3")
        shot("preferences")
        suffix = "-dark" if theme == "dark" else ""
        os.makedirs(os.path.join(out, lang), exist_ok=True)
        for name, data in shots.items():
            with open(os.path.join(out, lang, f"{name}{suffix}.webp"), "wb") as f:
                f.write(data)
        print(f"site screenshots: {lang} {theme}: {len(shots)} screens")
        return True
    except Exception as e:   # a timeout, a browser gone
        print(f"site screenshots: {lang} {theme}: {str(e).splitlines()[0]}")
        return False
    finally:
        context.close()


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) < 2:
        print(__doc__)
        return 2
    site, out = args[0], args[1]
    langs = args[2:] or sorted(os.path.basename(p)[:-5] for p in glob.glob("site/i18n/*.json"))
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(Handler, directory=site))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = f"http://127.0.0.1:{server.server_address[1]}/"
    failed = 0
    with sync_playwright() as p:
        for lang in langs:
            for theme in ("light", "dark"):
                # A browser for each run: SwiftShader's GPU process can stop
                # answering after a few starts in one.
                browser = p.chromium.launch(
                    executable_path=os.environ.get("PLAYWRIGHT_CHROMIUM") or None,
                    args=["--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"])
                try:
                    failed += not take(browser, base, lang, theme, out)
                finally:
                    browser.close()
    if "--png" in sys.argv and os.path.isdir(os.path.join(out, "en-US")):
        from PIL import Image
        for name in SCREENS:
            Image.open(os.path.join(out, "en-US", f"{name}.webp")).save(
                os.path.join("images", "screenshots", f"{name}.png"), optimize=True)
        print("site screenshots: images/screenshots/*.png written")
    server.shutdown()
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
