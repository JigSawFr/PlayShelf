# The web simulator

The desktop simulator (`make desktop`, `source/sim/`), compiled with
[Emscripten](https://emscripten.org) for a browser: the real Aurore UI
against a simulated console and a demo library server, to click through a
change, try a state or show a screen without a build on the console.
In the website's look: its colours, its header in Boréale's night whatever
the theme (the name opening the menu of Boréale's sites, the website's
language menu, the theme's menu, a short face of them on a phone) and Sora
for the name (`site/assets/fonts/sora.woff2`, with `branding/mark.svg` and the
sites' marks of `site/assets/marks/`, copied by CMake). The language menu is
the website's (`tools/build_site.py`, `site/assets/site.js`): a button with
the page's flag and language (its flag and code on a phone, its flag alone
under 380px), and under it the 15 languages in a popover, each with its
flag, the page's marked (Esc or a click elsewhere closes it, the focus back
on the button; ↓ on the button opens it at the language in use, the arrows,
Home and End move through it). The flags are the website's
`site/assets/fonts/twemoji-flags.woff2` (Twemoji's 15, CC-BY 4.0), copied by
CMake next to the page with `LICENSE-twemoji-flags.txt`, so Windows shows
them too.

## The demo library server

A browser cannot reach a library server without its CORS headers, nor speak
plain TCP: every request is answered as a console without a connection
(`source/sim/web_http.cpp`), but those to `https://demo.playshelf.example`,
a server packaged with the page (`/app/samples/demo`), and those to the
services that tell about games, answered about the demo's: nothing real is
fetched. All of it is made from one table, `tools/web_demo_games.json`: 28
made-up games like today's best known ones (an open world, a kart race,
monsters to collect, a village to live in…), of made-up series and studios,
none of a real game's names, characters or pictures. 24 are on the server,
4 are only coming out.

- **The catalogue**, a JSON index of the server's games, some with updates
  and add-ons, two as an NSZ, each with its icon.
- **The packages**, made by `tools/web_demo.cpp` with the install suites'
  builders (`tests/install/packages.hpp`): encrypted with the host tests'
  keys, which the simulated console takes (`source/sim/sim_install.cpp`).
  Small (what they install is a stand-in), served at `speed=` KB/s (96 by
  default, `PLAYSHELF_SIM_SPEED`; `0`: at once) so a download is seen going,
  byte ranges and resumes included: the downloads' queue, its split
  connections and its fetching ahead work on them as on a real server.
- **The pictures**, drawn the same at every build by `tools/web_demo_art.py`.
  - What they show: each game's icon (256 × 256), banner (854 × 480) and
    screenshots (1280 × 720). Each is a scene of its kind in its colours
    (islands in the sky, a kart track, a harbour village…), drawn as key art
    is: a sky, far shapes lost in the haze, a lit subject, light and grain
    over it all.
  - The icon is framed on its subject, with its name over it in Sora as a
    game's logo would be; a series' name sits above in a badge.
  - The screenshots carry a head-up display of their kind of game: hearts
    and a map, a race's place and lap, a battle's cards, a farm's day…
  - How: the scenes are SVG (`tools/web_demo_scenes.py`), rendered in
    Chromium through Playwright, then saved as JPEG with Pillow.
    `make web ART_PYTHON=…` picks the Python that has both;
    `CHROMIUM=…` uses another Chromium than Playwright's.
- **Nlib API's answers** (`https://api.nlib.cc/nx/…`, `nlib/`): each game's
  details in English and French (studio, release, genres, players,
  description), its icon, banner and screenshots, so a game's page is full.
- **A title database** (NXMeta's index, `https://nx-meta.nlib.cc/index.json`,
  `titledb/`): every game's name, studio and release, the coming ones among
  them, fetched once at the first start, so Home has its games coming soon.
- **PlayInsights' files** (`https://cdn.jsdelivr.net/gh/JigSawFr/PlayInsights@data/`,
  `insights/`: `popular.json`, `games.json` and `similar.json`, schema 1,
  made on the day of the build, newer than the files PlayShelf ships): Home
  ranks, recommends and groups the demo's games by their series and
  studios, each recommendation with its reason, and a game's page lists the
  ones like it.

The dates the table gives in `days` are counted from the day of the build:
the month's releases and the coming ones stay so for the weeks after it.

The page adds the server to `servers.json` on a first start (`no_demo=1`:
none, a console with no server yet), with sys-patch on the simulated SD
card, as a console that installs games has, and `PLAYSHELF_SIM_SIGNED=1`:
the demo's packages, signed with the test key, are taken as Nintendo's
(`signed=0`: each install asks first, as an unsigned package does). Six of
its games are on the console from that first start, played
(`source/sim/sim_play.cpp`), three on its wishlist, so Home has every row:
the page asks for them (`sim_console/seed_demo` in the data folder) and
PlayShelf puts their content meta records, names and icons and the
wishlist (`console/` of the demo) on the simulated console as it starts,
and fetches the title database (`source/sim/web_seed.cpp`).

## The page

- **The settings are in the URL**, each `PLAYSHELF_SIM_*` variable by its name
  in lower case without the prefix: `?applet=1&fw=20.5.0` is
  `PLAYSHELF_SIM_APPLET=1 PLAYSHELF_SIM_FW=20.5.0`
  ([docs/contributing/simulator.md](../docs/contributing/simulator.md) says what
  each one simulates). `tab` is not a variable but the first tab (`library`,
  `catalogue`, `preferences`, `about`; Home when unset), written into the simulated `config.json` before
  PlayShelf starts. A variable the panel does not know (one a newer build
  reads) still reaches PlayShelf.
- **The language** is the browser's, if the console has it (`fr-CA` for a
  Canadian browser, `es-419` for a Mexican one, `zh-Hant` for Taiwan…; else
  English), unless the URL says `lang=` or the header's language menu picks
  another (each language a link to the page with the same settings in it,
  which reloads it).
  The page is in the 15 languages of the console, and PlayShelf always
  starts in the page's.
- **The theme** is *Dark* by default, as PlayShelf on a console. The header
  chooses *Auto* (the browser's) or *Light* instead (kept for the next
  visits, and put in the URL), and `theme=` in the URL wins over both.
  PlayShelf reads it at start-up: when the system's changes while it runs,
  under *Auto*, the page offers to restart PlayShelf with it.
- **The side panel** shows the variables and changes them; **Apply** reloads
  the page with the new URL, so PlayShelf starts again from a fresh state, and
  **Copy link** gives a link that opens that state. **Presets** set common
  states in one go (a slow network, the SD card full, no server yet…).
- **Live settings**: the variables PlayShelf reads at each use (`live` in
  `params.json`: the demo server's speed, the free space, the update
  check's answers…) change without a restart when **Apply** changes nothing
  else (`playshelf_web_setenv()`, `source/sim/web_page.cpp`, kept by
  `core/sim_env.h` under a lock).
- **Keep the state** (`keep=1`): PlayShelf's data folder (`playshelf_data`:
  its settings, servers, downloads, and what the simulated console has
  installed) is kept in the browser's IndexedDB, written every 3 s and when
  the page is left. The console settings of the panel then count for a
  first start only; **Forget the kept state** starts over.
- **Controls**: the arrows, Enter (Ⓐ) and Esc (Ⓑ) reach SDL directly; the
  page presses the other buttons by what the key says: X, Y, L, R, Shift+L
  (ZL), Shift+R (ZR), + or P, − or M, Backspace (Ⓑ). A click is a touch.
  **Pad** shows an on-screen controller (by default on a touch screen):
  under the screen in portrait, on either side of it in landscape. **Full
  screen** shows only the screen and the pad. Added to a phone's home screen,
  it is "PlayShelf" in a window of its own; the icons come from
  `tools/render_branding.mjs` (`images/`).
- **Screenshot** saves the screen as it is (1280×720); **Record** saves a
  video of it (WebM) until **Stop**; **Log** shows what PlayShelf logs.
- **Steps**: from the first screen on, the page notes each button pressed,
  each touch and each live change, with the time between. **Steps link**
  gives a link with the settings PlayShelf started with and those steps
  (`rec=`); a page opening it replays them. A step is a button (`up`,
  `down`, `left`, `right`, `a`, `b`, `x`, `y`, `l`, `r`, `zl`, `zr`, `plus`,
  `minus`; `down*3` three times), `wN` (wait N tenths of a second), `tX,Y` (a
  touch, in 1280×720), `s:name=value` (a live setting) or `say:key` (a line
  of the tour); `runSteps()` plays them for the smoke test.
- **Tour** (`tour=1`): the Library, the demo server's Catalogue, a game
  installed, the Downloads tab and the Preferences, a line above the screen
  for each, in the page's language.
- **Report a problem** opens a new issue with the steps link, the build,
  the browser and the end of the log (as much as a URL takes).
- `<body data-playshelf>` says where PlayShelf is: `loading`, `running`,
  `ready` (its first screen is up, `start-up: first screen ready` in its
  log), `error`.

## How it is built

`make web` (needs Emscripten's `emcmake` on the `PATH`: `source
<emsdk>/emsdk_env.sh`, and libzstd and zlib for the demo server's packages)
builds `build-web/simulator/`; `make web-serve` serves it at
<http://localhost:8000>.

- `tools/web_demo.cpp`, built and run on the host first: the demo server's
  files in `build-web/demo/`.
- `CMakeLists.txt`, under `emcmake` (`PLAYSHELF_WEB`): the desktop build on
  Aurore's SDL2 platform with OpenGL ES 3, which Emscripten maps to WebGL 2;
  SDL2 and zlib from Emscripten's ports; zstd's decompressor from its
  release's sources (`FetchContent`, its hash pinned: Emscripten has no port
  of it); no libcurl (`source/sim/web_http.cpp` instead); the resources
  packaged at `/app/resources` and the demo server at `/app/samples/demo`
  (`playshelf.data`); a pool of 16 threads started with the page (a thread
  asked for later waits for the browser, which a busy main thread never
  lets it have); IDBFS for *Keep the state*; `source/sim/web_page.cpp`, what
  the page calls while PlayShelf runs (`ccall`).
- `source/main.cpp`: in a browser a frame must hand the thread back to the
  page, so the main loop is `emscripten_set_main_loop`.
- Aurore (`library/aurore`) runs in a browser: no SDL haptic, a window fixed
  at the console's 1280×720 (the page scales the canvas),
  `aurore_web_button()`, which the page calls to press a button, and the CJK
  font below as Aurore's fallback when it is there at start-up.
- **Japanese, Korean and Chinese**: a browser has no system font for
  Aurore to fall back on. The web build cuts the characters those four
  languages' catalogs use (PlayShelf's and Aurore's, for the theme names) out
  of Noto Sans CJK (SIL Open Font License,
  `tools/web_cjk_font.py`, which needs fontTools) into `cjk.otf`, served next
  to the page: in those four languages the page fetches it before PlayShelf
  starts; in the others, once PlayShelf runs, behind the Latin font
  (`playshelf_web_cjk_font()`). It takes
  `/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc` (Debian and
  Ubuntu's `fonts-noto-cjk`), or `-DPLAYSHELF_WEB_CJK_FONT=<file>`; without
  one, the build goes on with a warning and those languages show boxes.
- The page: `index.html`, `simulator.js`, `simulator.css`; `sim_logic.js`,
  what of it touches neither the page nor PlayShelf (the languages, the URL,
  the variables, the steps' format), tested with Node (`node --test
  tests/web_page/*.test.js`, CI); `params.json`, the panel's entries;
  `i18n/<language>.json`, its texts (`en-US.json` the reference).
  `tools/check_web_params.py` (CI) fails when a `PLAYSHELF_SIM_*` variable
  read in `source/` (or by Aurore's Cadran) has no entry, an entry no longer
  matches a variable, a language of the console has no catalog, or English
  lacks a text.
- **Threads need a cross-origin isolated page** (the COOP / COEP headers).
  GitHub Pages cannot send them, so `sw.js` adds them from a service worker,
  which reloads the page once on the first visit. `serve.py` sends them
  itself.
- **The next visits, and offline**: `sw.js` keeps PlayShelf's files
  (`playshelf.js`, `.wasm`, `.data`, `cjk.otf`, asked for with the commit in
  their URL) once fetched, and takes them from its cache from then on.
  `manifest.webmanifest` makes it installable as an app.
- **A Content-Security-Policy** (`index.html`'s `<meta>`): only the page's
  own files, no inline script or style; `'wasm-unsafe-eval'` compiles
  PlayShelf and `blob:` workers are Emscripten's threads.
- **The smoke test** (`tools/web_smoke.py`, CI's `web` workflow): the build
  in headless Chromium (Playwright, WebGL 2 from SwiftShader) must reach its
  first screen in each kind of start, go through every tab, install a game
  of the demo server, take a live setting without a restart, replay a steps
  link, play the tour, add the CJK font after the start, keep its data
  folder across a reload with `keep=1`, fit a phone, and, served without the
  headers as GitHub Pages does, be isolated by `sw.js`, keep PlayShelf's
  files and start offline. A page error fails it. Its scenarios run a few at
  once (`WEB_SMOKE_JOBS`); `WEB_SMOKE_ONLY=tour,keep` runs some of them.
  Locally, after `make web`: `python3 tools/web_smoke.py build-web/simulator
  build-web/shots` (with `pip install playwright` and `playwright install
  chromium`).

CI (`.github/workflows/web.yml`) builds it for main and for each pull
request, runs the smoke test and keeps the simulator and its screens as the
run's artifacts.
