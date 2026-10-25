# Tests and CI

Part of the [contributor's guide](../../CONTRIBUTING.md).

- `make test` — host unit tests (`tests/`): every suite runs, in parallel, with ASan, UBSan and libstdc++'s assertions, and leaves its output in `build/host-tests/<suite>.log` (`<suite>.ok` when it passed). A suite is three Makefile variables, `<name>_SRC`, `<name>_FLAGS` and `<name>_LIBS`; `tests/check.h` gives `CHECK`, `REQUIRE` and `CHECK_DONE`, `tests/temp_dir.hpp` a scratch folder, `tests/fs.hpp` small file helpers.
- `make fuzz` — libFuzzer runs of the parsers of outside data (`tests/fuzz/<name>/`: a seed corpus and an optional dictionary), `FUZZ_SECONDS` each (30 by default), with clang.
- `make check` — `make test`, then `tools/check_resources.py` (the catalogs, the layouts, the docs the app links to), `tools/check_sources.py` (the kit includes nothing of the app; the layers) and `tools/gen_compat.py`.
- `tools/desktop_smoke.py <out-dir> [loading]` — clicks through every screen of the desktop build headlessly (needs `DISPLAY`, `xdotool`, ImageMagick) and saves screenshots, plus `contact-sheet.png` with all of them. With no scenario it opens every tab, the pickers of Preferences, the quick settings, About, the developer mode, *Help & diagnostics* and the diagnostic report. The others:
  - `loading` — the loading screen, the start-up held at its last step.

CI (`.github/workflows/build.yml`) runs, for every pull request: the host tests with gcc (and the checks), the host tests with clang with coverage and a short fuzz run, the desktop build and its smoke test, and the Switch build (`make dist` with warnings as errors) in devkitPro's container, pinned by digest. A pull request's title is checked too (`pr-title.yml`).
