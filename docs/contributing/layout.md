# Code layout

Part of the [contributor's guide](../../CONTRIBUTING.md).

PlayShelf is built on the technical base of [PlayGuard](https://github.com/JigSawFr/PlayGuard). What the two apps share lives in **`source/kit/`**; what is PlayShelf's own lives beside it, in `source/`.

| Folder | What it holds |
|---|---|
| `source/kit/core/` | The C layer over libnx (platform, system information, local time, crash handler, start-up timing, the read-only switch). Each module has a plain-C header; the desktop build replaces the console's ones with `source/kit/sim/` |
| `source/kit/sim/` | The simulated console of the desktop build, driven by `PLAYSHELF_SIM_*` variables ([the simulator](simulator.md)) |
| `source/kit/util/` | Plain C++, most of it host-tested: HTTP (libcurl, TLS verified; byte ranges streamed), JSON, SHA-256, AES-128, secrets sealed for this console (`secret_box`, with `core/device_key.h`), zip, paths and atomic writes, migrations, session logs and crash reports, updates, development builds, GitHub sign-in, diagnostics |
| `source/kit/action/` | Flows: `*_logic` files are plain C++ (host-tested), `*_flow` files drive the dialogs and screens |
| `source/kit/ui/`, `source/kit/view/` | The Cadran look over borealis: the frame (header, rail), dialogs and pickers, lists, palettes, icons, effects |
| `source/kit/activity/`, `source/kit/tab/` | The shared screens (loading, update, development builds, diagnostics, changelog, what's new) and the base of every tab |
| `source/install/` | The install formats, plain C++ with no libnx nor borealis, host-tested and fuzzed (`tests/install/`, `tests/fuzz/install_*`): AES-128 (XTS with Nintendo's tweak, CTR, ECB), RSA-2048-PSS, the PFS0 and HFS0 archives (an NSP, a game card's partitions) and a game card's header, a title's content meta (CNMT) and the record ncm keeps of it, an NCA's header and the content meta inside a `.cnmt.nca`, and a content as it arrives (`content_stream`: a `.nca`, or a `.ncz` decompressed with zstd and encrypted again) made the bytes of its NCA, checked against the content meta's SHA-256; a package (`package`: an NSP, NSZ, XCI or XCZ, read through a `Source` of ranges) installed as one transaction (`installer`: everything checked before anything is written, every failure undone) on a `Console` interface (ncm, its content meta database, ns and es). The console's keys come from outside (`NcaKeys`) |
| `source/library/` | The library client's formats, plain C++ host-tested and fuzzed (`tests/library/`, `tests/fuzz/library_*`): the catalogue of an AeroFoil/Ownfoil API answer (`/api/remote`, `/api/shop`) and of a JSON index, addresses made absolute and https only, title ids and kinds, a LAN discovery answer, which of a server's addresses to use, a certificate's public-key pin |
| `source/core/` | PlayShelf's own C layer over libnx: `install.h`, the services an install goes through (ncm's content storage and content meta database, ns's application records, es's tickets, spl's NCA keys). Switch only; the desktop build swaps in `source/sim/` |
| `source/sim/` | The desktop build's console for `source/core/`: `sim_install.cpp`, a console in `playshelf_data/sim_console/` with the host tests' keys |
| `source/action/` | PlayShelf's own flows: `install_flow` (Help & diagnostics › developer tools › Install from a URL) |
| `source/` (outside `kit/`) | PlayShelf itself: `main.cpp`, `app.*` (its identity and modes), `util/config.*` (its settings), its screens and tabs |

The rules `tools/check_sources.py` enforces:

- **The kit includes nothing of the app** but `app.hpp` and `util/config.hpp` (the app's identity and settings). It asks anything else of the app through `ui::hooks` (`source/kit/ui/hooks.hpp`), which `source/activity/screen_hooks.cpp` fills in at start-up: the main screen, the tabs' positions and rail icons, the notifications, a check before PlayShelf replaces itself. The kit can then become a library shared with PlayGuard without untangling anything.
- `ui/`, `view/` and `util/` never include `activity/` or `tab/`; a flow includes only the screens it opens itself.
- A C file of `source/kit/core/` that needs no console service is in the desktop build and in a host test.

The Cadran look: a colour is always a palette key (`cadran::kit::color("cadran/teal")`, `@theme/cadran/...` in XML), never a literal; `tests/contrast` checks every ink on every surface in both themes.
