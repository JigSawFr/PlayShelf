"""Which Chromium the tools that draw pages (site_screenshots.py, web_smoke.py,
web_demo_art.py) start through Playwright.

In this order: $PLAYWRIGHT_CHROMIUM, or $CHROMIUM; none (Playwright's own, when
it is installed); a Chromium or Chrome of the system (chromium,
chromium-browser, google-chrome in the PATH). Playwright's own is the one its
version expects: a system browser is only the way out when it is not there
(a Playwright newer than the browser installed, a container with a browser
and no download).
"""
import os
import shutil


def executable(playwright):
    """The `executable_path` for chromium.launch(), None for Playwright's own.
    `playwright` is what sync_playwright() gives."""
    chosen = os.environ.get("PLAYWRIGHT_CHROMIUM") or os.environ.get("CHROMIUM")
    if chosen:
        return chosen
    try:
        if os.path.exists(playwright.chromium.executable_path):
            return None
    except Exception:
        pass
    for name in ("chromium", "chromium-browser", "google-chrome"):
        found = shutil.which(name)
        if found:
            return found
    return None
