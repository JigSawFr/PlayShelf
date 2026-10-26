# Contributing to PlayShelf

Thanks for helping! This page is the map; each subject has a page of its own under [`docs/contributing/`](docs/contributing/).

## In short

```sh
git clone --recursive https://github.com/JigSawFr/PlayShelf && cd PlayShelf
make test        # the host tests: gcc, g++, zlib headers, no devkitPro
make desktop     # the real UI against a simulated console: GLFW, X11 and D-Bus dev packages
make dist        # playshelf.zip for the SD card: devkitPro's switch-dev
```

Then a pull request with a conventional title and a scope (`fix(install): …`), squash-merged.

## The pages

| Page | What it covers |
|---|---|
| [Build from source](docs/contributing/build.md) | Every `make` target and what it needs; the borealis submodule (Aurore) |
| [The desktop simulator](docs/contributing/simulator.md) | `make desktop` against a simulated console, every `PLAYSHELF_SIM_*` variable |
| [The web simulator](web/README.md) | `make web`: the desktop simulator in a browser, with a demo library server |
| [Tests and CI](docs/contributing/tests.md) | The host tests, the fuzz targets, the smoke test, what CI runs |
| [Code layout](docs/contributing/layout.md) | The kit shared with PlayGuard and its rules, the Cadran look |
| [Pull requests and releases](docs/contributing/releases.md) | Conventional titles, squash merges, release-please, development builds, reproducible zips |
| [Translating PlayShelf](docs/contributing/translating.md) | The 15 catalogs, how to add a language |

The settings file: [`docs/config.md`](docs/config.md).

## What PlayShelf will not do

PlayShelf installs content you own, from a server you run. Contributions that decrypt Tinfoil indexes, imitate Tinfoil's authentication, send hardware identifiers to servers, turn TLS verification off, or target a service that distributes games without the rights to do so are not accepted.
