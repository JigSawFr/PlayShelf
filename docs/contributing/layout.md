# Code layout

Part of the [contributor's guide](../../CONTRIBUTING.md).

PlayShelf is built on the technical base of [PlayGuard](https://github.com/JigSawFr/PlayGuard). What the two apps share lives in **`source/kit/`**; what is PlayShelf's own lives beside it, in `source/`.

| Folder | What it holds |
|---|---|
| `source/kit/core/` | The C layer over libnx (platform, system information, local time, crash handler, start-up timing, the read-only switch). Each module has a plain-C header; the desktop build replaces the console's ones with `source/kit/sim/` |
| `source/kit/sim/` | The simulated console of the desktop build, driven by `PLAYSHELF_SIM_*` variables ([the simulator](simulator.md)) |
| `source/kit/util/` | Plain C++, most of it host-tested: HTTP (libcurl, TLS verified), JSON, SHA-256, zip, paths and atomic writes, migrations, session logs and crash reports, updates, development builds, GitHub sign-in, diagnostics |
| `source/kit/action/` | Flows: `*_logic` files are plain C++ (host-tested), `*_flow` files drive the dialogs and screens |
| `source/kit/ui/`, `source/kit/view/` | The Cadran look over borealis: the frame (header, rail), dialogs and pickers, lists, palettes, icons, effects |
| `source/kit/activity/`, `source/kit/tab/` | The shared screens (loading, update, development builds, diagnostics, changelog, what's new) and the base of every tab |
| `source/` (outside `kit/`) | PlayShelf itself: `main.cpp`, `app.*` (its identity and modes), `util/config.*` (its settings), its screens and tabs |

The rules `tools/check_sources.py` enforces:

- **The kit includes nothing of the app** but `app.hpp` and `util/config.hpp` (the app's identity and settings). It asks anything else of the app through `ui::hooks` (`source/kit/ui/hooks.hpp`), which `source/activity/screen_hooks.cpp` fills in at start-up: the main screen, the tabs' positions and rail icons, the notifications, a check before PlayShelf replaces itself. The kit can then become a library shared with PlayGuard without untangling anything.
- `ui/`, `view/` and `util/` never include `activity/` or `tab/`; a flow includes only the screens it opens itself.
- A C file of `source/kit/core/` that needs no console service is in the desktop build and in a host test.

The Cadran look: a colour is always a palette key (`cadran::kit::color("cadran/teal")`, `@theme/cadran/...` in XML), never a literal; `tests/contrast` checks every ink on every surface in both themes.
