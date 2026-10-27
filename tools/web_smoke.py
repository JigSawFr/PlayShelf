#!/usr/bin/env python3
"""Smoke test of the web simulator in a browser (web/README.md).

Serves the built simulator (build-web/simulator/) with the headers threads
need, opens it in headless Chromium (Playwright) and fails when PlayShelf does
not reach its first screen ("start-up: first screen ready" in its log, which
the page says in <body data-playshelf="ready">), when the page throws, or
when what the page does while PlayShelf runs does not work:

- a page in each kind of start: the Library, a tab of its own (tab=), no
  library server (no_demo=1), without Atmosphère;
- every tab in turn, played as steps (runSteps(), web/simulator.js), the
  demo server's Catalogue among them;
- a game of the demo server installed: its sheet, Install everything, its
  packages downloaded and installed, the game in the Library;
- a `live` setting changed from the panel without a restart;
- the steps: keys pressed, then the steps link replayed; the tour; a report
  link to the bug form;
- the CJK font: waited for in Japanese, added after the start in English;
- keep=1: the data folder is still there after a reload;
- on a phone (an iPhone, touch): the pad under the screen in portrait, on
  either side of it in landscape, everything on the screen, no field iOS
  zooms into (under 16px); a "Script error." not of the page's (iOS'
  share sheet) does not stop it; Full screen: only the screen and the pad,
  and back;
- the service worker (web/sw.js), served without the headers as GitHub Pages
  does: it makes the page cross-origin isolated, keeps PlayShelf's files, and
  the simulator starts offline.

Each of them in a process and browser of its own, a few at once
(WEB_SMOKE_JOBS, the machine's processors by default: PlayShelf runs on the
CPU, WebGL through SwiftShader), and stopped when it takes longer than any
should (SCENARIO_S): a browser that stopped answering fails its scenario
rather than the job's timeout.

The screens are written to <out-dir>/<name>.png (1280x720, the canvas), for
the pull request's preview comment (.github/workflows/pages.yml), and the
time each start took to <out-dir>/timings.json. The screens show one frozen
moment (PLAYSHELF_SIM_NOW) with the focus glow still.

Usage: tools/web_smoke.py <simulator-dir> <out-dir>   (needs Playwright for
Python with Chromium; PLAYWRIGHT_CHROMIUM=<path> for a browser of its own)
"""
import base64
import functools
import http.server
import json
import os
import signal
import subprocess
import sys
import tempfile
import threading
import time

from playwright.sync_api import sync_playwright

SITE, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)

# The same moment for every screen (Thursday 8 October 2026, 15:00 in Paris),
# the focus still, the header's badge on one build, the page in English.
FIXED = "lang=en-US&theme=light&now=1791471600&still_focus=1&version=1.0.0:1a2b3c4"
# What a start may take on a CI runner (software WebGL): the download, the
# intro (2 s) and the start-up itself.
START_S = 120
# What a scenario may take at most: a start, its steps, its screens (the tour
# plays for about a minute), with the others on the other cores.
SCENARIO_S = 300
# build.json for the service worker's server: a commit in the files' URLs.
BUILD = {"repo": "JigSawFr/PlayShelf", "sha": "0000000smoke", "ref": "smoke", "title": "", "pr": None}


class Handler(http.server.SimpleHTTPRequestHandler):
    """The site, with the COOP / COEP headers (`isolate`), else as GitHub
    Pages serves it, with a build.json."""
    isolate = True

    def do_GET(self):
        if not self.isolate and self.path.split("?")[0] == "/build.json":
            body = json.dumps(BUILD).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        super().do_GET()

    def end_headers(self):
        if self.isolate:
            self.send_header("Cross-Origin-Opener-Policy", "same-origin")
            self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def log_message(self, *args):
        pass


class PagesHandler(Handler):
    isolate = False


class Server(http.server.ThreadingHTTPServer):
    def handle_error(self, request, client_address):
        if not isinstance(sys.exc_info()[1], (ConnectionResetError, BrokenPipeError)):
            super().handle_error(request, client_address)   # a page left mid-download is no error


def serve(handler, host):
    server = Server((host, 0), functools.partial(handler, directory=SITE))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, f"http://{host}:{server.server_address[1]}/"


# A scenario's own process (below) uses the servers of the run that started it.
CHILD = os.environ.get("WEB_SMOKE_CHILD")
if CHILD:
    BASE, PAGES = os.environ["WEB_SMOKE_BASE"], os.environ["WEB_SMOKE_PAGES"]
else:
    server, BASE = serve(Handler, "127.0.0.1")
    # Another origin, so its service worker starts from nothing.
    pages_server, PAGES = serve(PagesHandler, "localhost")

failures = []
timings = {}


def fail(msg):
    print("WEB SMOKE FAILED:", msg)
    failures.append(msg)


def log_text(page):
    """What PlayShelf and the page logged (simulator.js' log lines)."""
    return page.evaluate("logLines.map(([line]) => line).join('\\n')")


def wait_logged(page, text, seconds):
    end = time.time() + seconds
    while time.time() < end:
        if text in log_text(page):
            return True
        time.sleep(0.25)
    return False


def open_page(page, query, what, base=None):
    """Opens the simulator with `query`; True once its first screen is up."""
    t0 = time.time()
    page.goto(f"{base or BASE}?{query}")
    # The service worker reloads the page once (web/sw.js): waiting goes on
    # in the new one.
    while True:
        left = START_S - (time.time() - t0)
        try:
            page.wait_for_function("document.body && ['ready', 'error'].includes(document.body.dataset.playshelf)",
                                   timeout=max(left, 1) * 1000)
            break
        except Exception as e:
            if left > 0 and ("navigat" in str(e) or "destroyed" in str(e)):
                continue
            fail(f"{what}: no first screen in {START_S} s ({str(e).splitlines()[0]})")
            try:
                print(log_text(page)[-3000:])
            except Exception:
                pass
            return False
    if page.evaluate("document.body.dataset.playshelf") == "error":
        fail(f"{what}: {page.inner_text('#overlay')}")
        print(log_text(page)[-3000:])
        return False
    timings[what] = round(time.time() - t0, 2)
    return True


def steps(page, text):
    """Plays steps (web/simulator.js' runSteps()) and waits for their end."""
    page.evaluate("(text) => runSteps(text)", text)


def shot(page, name):
    # A moment for the screen to settle (borealis' animations last 300 ms),
    # and as long again as borealis still holds the input (a screen opening
    # on a busy runner: the footer's hints greyed), up to 5 s; then, two
    # frames later, the canvas as PlayShelf drew it.
    time.sleep(1.5)
    end = time.time() + 5
    while time.time() < end and page.evaluate("Module.ccall('playshelf_web_busy', 'number', [], [])"):
        time.sleep(0.1)
    page.evaluate("new Promise((done) => requestAnimationFrame(() => requestAnimationFrame(done)))")
    data = page.evaluate("canvas.toDataURL('image/png')")
    with open(os.path.join(OUT, name + ".png"), "wb") as f:
        f.write(base64.b64decode(data.split(",", 1)[1]))


errors = []
SCENARIOS = []


def scenario(fn):
    SCENARIOS.append(fn)
    return fn


def run(fn):
    """One scenario, in a Playwright and browsers of its own (its thread's).
    A browser for each start, the last one closed: on a runner, SwiftShader's
    GPU process can crash after a few starts in one browser, which then
    answers nothing more (an evaluate() waits forever)."""
    with sync_playwright() as p:
        browsers = []

        def new_page(**options):
            while browsers:
                browsers.pop().close()
            browser = p.chromium.launch(
                executable_path=os.environ.get("PLAYWRIGHT_CHROMIUM") or None,
                # WebGL 2 without a GPU: SwiftShader.
                args=["--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"])
            browsers.append(browser)
            # The page allows no eval (its Content-Security-Policy); the test's
            # own wait_for_function needs one.
            context = browser.new_context(**{"viewport": {"width": 1600, "height": 1000}, "locale": "en-US",
                                             "bypass_csp": True, **options})
            page = context.new_page()
            page.on("pageerror", lambda e: errors.append(str(e)))
            return page

        try:
            fn(new_page)
        except Exception as e:   # a timeout, a browser gone: the scenario fails, not the others
            fail(f"{fn.__name__}: {str(e).splitlines()[0]}")
        finally:
            for browser in browsers:
                browser.close()


# desktop_smoke's tour of the tabs, each one in turn from the sidebar: the
# demo server's Catalogue on the way.
@scenario
def tabs(new_page):
    page = new_page()
    if open_page(page, FIXED, "tabs"):
        shot(page, "library")
        steps(page, "left")
        for name in ["catalogue", "downloads", "preferences", "about"]:
            steps(page, "down")
            if name == "catalogue":
                time.sleep(2)   # its catalogue and icons, from the demo server
            shot(page, name)
        if page.evaluate("document.body.dataset.playshelf") != "ready":
            fail("tabs: PlayShelf stopped on the way")
        else:
            print("ok   every tab")


# A game of the demo server installed: its sheet, Install everything, its
# two packages (the base game and an update) installed one after the
# other; then the Downloads tab and the game in the Library.
@scenario
def install(new_page):
    page = new_page()
    if not open_page(page, FIXED + "&tab=catalogue&speed=0", "install"):
        return
    time.sleep(2)   # the catalogue, from the demo server
    steps(page, "right.down.a")
    shot(page, "game_sheet")
    steps(page, "a")
    if not wait_logged(page, "downloads: Copper Canyon Rally · Update v65536 installed", 30):
        fail("install: the game's packages were not installed")
        print(log_text(page)[-2000:])
        return
    steps(page, "b.left.down")
    shot(page, "downloads_done")
    steps(page, "up*2")
    shot(page, "library_installed")
    print("ok   a game installed from the demo server")


@scenario
def no_server(new_page):
    page = new_page()
    if open_page(page, FIXED + "&no_demo=1&tab=catalogue", "no_server"):
        shot(page, "no_server")
        print("ok   no_server")


@scenario
def no_cfw(new_page):
    page = new_page()
    if open_page(page, FIXED + "&no_cfw=1", "no_cfw"):
        shot(page, "no_cfw")
        print("ok   no_cfw")


# A live setting from the panel (its group folded until then): no reload,
# the URL and the log follow. The CJK font, after the start in English.
@scenario
def live(new_page):
    page = new_page()
    if open_page(page, FIXED, "live"):
        page.evaluate("window.notReloaded = true")
        page.evaluate("document.querySelector('[data-group=library]').open = true")
        page.fill("[data-name=speed]", "16")
        page.click("#apply")
        if not wait_logged(page, "live: speed=16", 5) or not page.evaluate("window.notReloaded === true"):
            fail("live: speed from the panel did not reach PlayShelf without a restart")
        elif "speed=16" not in page.url:
            fail(f"live: the URL does not say speed=16 ({page.url})")
        else:
            print("ok   live setting from the panel")
        if not wait_logged(page, "Using zh-Hans font", 30):
            fail("cjk: the font was not added after the start")
        else:
            print("ok   CJK font added after the start")


# The steps: two presses noted, their link replays them. A start on a tab
# of its own (tab=).
@scenario
def steps_link(new_page):
    page = new_page()
    if open_page(page, FIXED + "&tab=catalogue", "steps"):
        page.focus("#canvas")
        for key in ("ArrowDown", "ArrowDown"):
            time.sleep(0.5)
            page.keyboard.press(key)
        link = page.evaluate("stepsLink()")
        rec = link.split("rec=", 1)[1].split("&")[0] if "rec=" in link else ""
        if rec.split(".").count("down") != 2:
            fail(f"steps: the link does not hold the presses ({link})")
        elif open_page(page, link.split("?", 1)[1], "steps (replayed)") and not wait_logged(page, "steps: done", 30):
            fail("steps: the link's steps were not replayed")
        elif "rec=" in page.url:
            fail(f"steps: rec= stayed in the URL ({page.url})")
        else:
            print("ok   steps link replayed")
        # A report: a new issue, the steps link in its body.
        url = page.evaluate("() => { let u = ''; window.open = (x) => { u = x; }; report(); return u; }")
        if "/issues/new?" not in url or "rec%3D" not in url or len(url) > 8000:
            fail(f"report: not a new issue with the steps ({len(url)} characters)")
        else:
            print("ok   report link")


@scenario
def tour(new_page):
    page = new_page()
    if open_page(page, FIXED + "&tour=1", "tour"):
        if not wait_logged(page, "steps: done", 120) or "installed" not in log_text(page):
            fail("tour: it did not play to its end, a game installed on the way")
        else:
            print("ok   tour")


# The CJK font waited for in Japanese.
@scenario
def japanese(new_page):
    page = new_page()
    if open_page(page, "lang=ja&theme=light&now=1791471600&still_focus=1&version=1.0.0:1a2b3c4", "japanese"):
        log = log_text(page)
        if "Using zh-Hans font" not in log or log.index("Using zh-Hans font") > log.index("start-up: first screen ready"):
            fail("japanese: the CJK font was not there at start-up")
        else:
            shot(page, "japanese")
            print("ok   japanese")


# keep=1: the data folder survives a reload. A file of the test's own, since
# the page writes config.json again at each start: only one written before
# the reload says the page kept the folder.
@scenario
def keep(new_page):
    page = new_page()
    if open_page(page, FIXED + "&keep=1", "keep"):
        page.evaluate("Module.FS.writeFile('/playshelf_data/smoke_kept', 'kept')")
        time.sleep(4)   # the page writes IndexedDB every 3 s
        page.evaluate("saveData()")
        time.sleep(1)
        if open_page(page, FIXED + "&keep=1", "keep (again)"):
            kept = page.evaluate("Module.FS.readdir('/playshelf_data')")
            if "smoke_kept" not in kept or "servers.json" not in kept:
                fail(f"keep: the data folder was not kept across a reload ({kept})")
            else:
                print("ok   keep")


# On a phone: the page laid out for a finger, in portrait and in landscape
# (web/simulator.css), the pad shown by default on a touch screen.
LAYOUT = """() => {
  const box = (s) => { const r = document.querySelector(s).getBoundingClientRect(); return [r.left, r.top, r.right, r.bottom]; };
  const small = [...document.querySelectorAll('input, select, textarea')]
    .filter((e) => e.type !== 'checkbox' && e.offsetParent && parseFloat(getComputedStyle(e).fontSize) < 16)
    .map((e) => e.id || e.name || e.tagName);
  return { wide: document.documentElement.scrollWidth > innerWidth + 1, small, screen: box('.screen'),
           dpad: box('.dpad'), face: box('.face'), shown: !document.querySelector('#pad').hidden };
}"""


@scenario
def phone(new_page):
    page = new_page(viewport={"width": 402, "height": 874}, is_mobile=True, has_touch=True)
    if not open_page(page, FIXED, "phone"):
        return
    m = page.evaluate(LAYOUT)
    if not m["shown"]:
        return fail("phone: no pad on a touch screen")
    problems = []
    if m["wide"]:
        problems.append("portrait: wider than the screen")
    if m["small"]:
        problems.append(f"fields under 16px (iOS zooms into them): {', '.join(m['small'])}")
    if not m["screen"][3] <= m["dpad"][1]:
        problems.append("portrait: the pad is not right under the screen")
    page.set_viewport_size({"width": 874, "height": 402})
    m = page.evaluate(LAYOUT)
    if m["wide"]:
        problems.append("landscape: wider than the screen")
    if not (m["dpad"][2] <= m["screen"][0] and m["face"][0] >= m["screen"][2]):
        problems.append("landscape: the pad is not on either side of the screen")
    if max(m["screen"][3], m["dpad"][3], m["face"][3]) > 402:
        problems.append("landscape: the screen or the pad below the screen's bottom")
    # What iOS' share sheet (Add to Home Screen) can leave the page: an error
    # of a script it may not read. Not PlayShelf's: it goes on.
    page.evaluate("window.dispatchEvent(new ErrorEvent('error', {message: 'Script error.'}))")
    if page.evaluate("document.body.dataset.playshelf") == "error":
        problems.append("a 'Script error.' not of the page's stopped PlayShelf")
    # Full screen (Android): only the screen and the pad, then back.
    page.tap("#fullscreen")
    page.wait_for_function("document.documentElement.classList.contains('game')", timeout=5000)
    if page.is_visible(".top") or page.is_visible(".controls"):
        problems.append("full screen: the header or the tools still shown")
    page.tap("#fs-exit")
    page.wait_for_function("!document.documentElement.classList.contains('game')", timeout=5000)
    if not page.is_visible(".top"):
        problems.append("full screen left: no header")
    for p in problems:
        fail(f"phone: {p}")
    if not problems:
        print("ok   phone: portrait, landscape and full screen")


# The service worker, as on GitHub Pages: no headers from the server.
@scenario
def service_worker(new_page):
    sw_page = new_page()
    sw_context = sw_page.context
    if open_page(sw_page, FIXED, "service worker", PAGES):
        if not sw_page.evaluate("self.crossOriginIsolated && !!navigator.serviceWorker.controller"):
            fail("service worker: the page is not isolated by it")
        else:
            print("ok   service worker: isolated")
        # Its own cache, named after its scope (web/sw.js).
        cached = sw_page.evaluate(
            "navigator.serviceWorker.ready.then((r) => caches.open('playshelf-simulator:' + new URL(r.scope).pathname))"
            ".then((c) => c.keys()).then((k) => k.map((r) => r.url))")
        if not any("playshelf.wasm?v=" in u for u in cached) or not any("playshelf.data?v=" in u for u in cached):
            fail(f"service worker: PlayShelf's files are not kept ({cached})")
        else:
            print("ok   service worker: PlayShelf's files kept")
        sw_context.set_offline(True)
        if open_page(sw_page, FIXED, "offline", PAGES):
            print("ok   offline")
        sw_context.set_offline(False)


if CHILD:
    # One scenario (WEB_SMOKE_CHILD), its results in a file for the run.
    name, result = CHILD.split(":", 1)
    run(next(fn for fn in SCENARIOS if fn.__name__ == name))
    with open(result, "w") as f:
        json.dump({"failures": failures, "timings": timings, "errors": errors}, f)
    sys.exit(0)

# The longest first, so that they do not end the run alone. WEB_SMOKE_ONLY:
# some of them, by name (unlock_error,tour).
SCENARIOS.sort(key=lambda fn: fn not in (tour, tabs))
if os.environ.get("WEB_SMOKE_ONLY"):
    SCENARIOS = [fn for fn in SCENARIOS if fn.__name__ in os.environ["WEB_SMOKE_ONLY"].split(",")]
jobs = max(1, int(os.environ.get("WEB_SMOKE_JOBS") or os.cpu_count() or 2))
results = tempfile.mkdtemp()
queue, running = list(SCENARIOS), []
while queue or running:
    while queue and len(running) < jobs:
        fn = queue.pop(0)
        result = os.path.join(results, fn.__name__ + ".json")
        # In a process group of its own, with its browsers: stopped all at once.
        proc = subprocess.Popen([sys.executable, "-u", __file__, SITE, OUT], start_new_session=True,
                                env={**os.environ, "WEB_SMOKE_CHILD": f"{fn.__name__}:{result}",
                                     "WEB_SMOKE_BASE": BASE, "WEB_SMOKE_PAGES": PAGES})
        running.append((proc, fn, result, time.time()))
    time.sleep(0.5)
    for item in list(running):
        proc, fn, result, started = item
        if proc.poll() is None:
            if time.time() - started <= SCENARIO_S:
                continue
            # The scenario and its browsers: Playwright closes them on
            # SIGTERM; what is left, killed.
            os.killpg(proc.pid, signal.SIGTERM)
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait()
            fail(f"{fn.__name__}: still running after {SCENARIO_S} s, stopped")
        elif os.path.exists(result):
            with open(result) as f:
                r = json.load(f)
            failures += r["failures"]
            timings.update(r["timings"])
            errors += r["errors"]
        else:
            fail(f"{fn.__name__}: its process ended without a result ({proc.returncode})")
        running.remove(item)
for e in errors:
    fail(f"page error: {e}")

server.shutdown()
pages_server.shutdown()
with open(os.path.join(OUT, "timings.json"), "w") as f:
    json.dump(timings, f, indent=1)
print("starts (s): " + ", ".join(f"{k} {v}" for k, v in timings.items()))
if failures:
    print(f"{len(failures)} failure(s)")
    sys.exit(1)
print("web smoke test: ok")
