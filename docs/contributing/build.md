# Build from source

Part of the [contributor's guide](../../CONTRIBUTING.md).

| Command | What it does | Needs |
|---|---|---|
| `make test` | Host unit tests of the kit's console-free C and of the UI-free C++ helpers and flow logic, with ASan + UBSan (`SAN=` to turn them off) and libstdc++'s assertions; the suites run in parallel, all of them even when one fails | gcc and g++ with the sanitizers, zlib and zstd headers, the Aurore submodule — no devkitPro |
| `make desktop` | The real UI on Linux, against a simulated console and the demo's made-up games ([the simulator](simulator.md)), with Aurore's debugging aids read from the environment (`AURORE_INSPECTOR=1`, `AURORE_HOT_RELOAD=<resources folder>`) | GLFW / X11 / Wayland / D-Bus dev packages, libzstd, and Playwright for Python with Pillow for the demo's pictures (`DESKTOP_DEMO=`: without the demo) |
| `make` | `./playshelf.nro` — drawn with deko3d (`GL=1` for OpenGL) | devkitPro `switch-dev`, `DEVKITPRO` set |
| `make dist` | `./playshelf.zip` (SD-card layout, reproducible) | as above |
| `make check` | `make test`, then the resource, source and compatibility checks ([tests](tests.md)) | as `make test`, Python 3 |
| `make fuzz` | libFuzzer runs of the parsers of outside data ([tests](tests.md)) | clang with libFuzzer |
| `make web`, `make web-serve` | [The web simulator](../../web/README.md) in `build-web/simulator/`, then served at <http://localhost:8000> | Emscripten (`emcmake`), libzstd and zlib; Playwright for Python with Chromium, and Pillow, for the demo's pictures |
| `make site` | [The website](../../site/README.md) in `build-site/`, its links checked, with the simulator once `make web` built it, served at <http://localhost:8000> | Python 3 and Python-Markdown |
| `make nxlink` | `make`, then sends `playshelf.nro` to a console's homebrew menu over the network | as `make`, `nxlink` |
| `make clean` | Deletes the Switch and desktop builds and what `make dist` made | |
| `./run.sh [ip]` | Builds in the `devkitpro/devkita64` Docker image, at the digest CI uses (read from `build.yml`), optionally `nxlink`s to a console | Docker |

Clone with submodules (`git clone --recursive`, or `git submodule update --init --recursive`): Aurore is pinned at `library/aurore/`: [Aurore](https://github.com/JigSawFr/Aurore) (`main`), our fork of xfangfang/borealis, with the fixes PlayGuard and PlayShelf need in it (one commit each, its message says why; those that were patches name theirs, `patch 0007`) and a few taken from borealis' other forks. A change to it is a pull request there, then a submodule bump here.

On Debian or Ubuntu, the desktop build needs `cmake libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxkbcommon-dev libwayland-dev wayland-protocols libdbus-1-dev libcurl4-openssl-dev zlib1g-dev libzstd-dev`; its smoke test `xvfb xdotool imagemagick dbus`.
