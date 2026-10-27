# The desktop simulator

Part of the [contributor's guide](../../CONTRIBUTING.md).

`make desktop` runs the real borealis UI against `source/kit/sim/`, a simulated console. Environment knobs (a flag is on with any value but empty, `0`, `false`, `off` or `no`; a number out of range is brought within it):

| Variable | Simulates |
|---|---|
| `PLAYSHELF_SIM_APPLET=1` | Applet (album) mode |
| `PLAYSHELF_SIM_BACKGROUND=1` | PlayShelf out of focus |
| `PLAYSHELF_SIM_BATTERY=1` | The console in hand, off its dock (unset: the simulator draws the effects as on the dock, whatever `PLAYSHELF_SIM_DOCKED` says): *Visual effects › Automatic*, the default, leaves out the glow and every movement of the look (`ui/effects`, `ui/motion`) |
| `PLAYSHELF_SIM_BLANK=1` | Atmosphère blanking the serial number |
| `PLAYSHELF_SIM_BOOT_CHOICE=a` | The answer of the start-up screen that offers the previous build after a start that did not finish (`a`: A, else B) |
| `PLAYSHELF_SIM_CADRAN_NOGLOW=1` | Not the console: no background glow the custom draw of the look after the design boards (Aurore's Cadran, `library/aurore/library/lib/cadran/`: the rail's icons, the figures in Sora, OFL), to weigh it |
| `PLAYSHELF_SIM_CUT_TEXT=1` | Not the console: once a second, every label the screen on top cuts short ("…") goes to `cut_text.txt` in the working directory with the screen's name (`library › dialog`, `… › picker` for one opened over it), once each (`tools/cut_text_check.py`) |
| `PLAYSHELF_SIM_DEV_BUILDS=<folder>` | What the GitHub API returns for *Install another build*: `releases.json` (the releases tab), `latest.json` (the latest release, for *Update in place*), `artifacts.json`, `pulls.json`; `runs.json`, and for *Find a build* `pull_<n>.json`, `commit.json`, `run_artifacts.json`; `https://<local path>` download URLs are copied from disk (`offline` for no network); `PLAYSHELF_SIM_DEV_BUILDS_DELAY=<ms>` slows each answer |
| `PLAYSHELF_SIM_DOCKED=1` | The console on its dock: *Text size › Automatic* draws the larger text |
| `PLAYSHELF_SIM_EMUMMC=1` | Running on emuMMC |
| `PLAYSHELF_SIM_FOCUS_LOG=1` | Not the console: each line or button that takes the focus logs its words (`focus: Preferences`), so `tools/desktop_smoke.py` reaches a line by its words (`go_to()`), not by counting presses |
| `PLAYSHELF_SIM_FW=20.5.0` | Another firmware (default 23.0.1) |
| `PLAYSHELF_SIM_FREE_MB=0` | The simulated console's free space for an install, in MiB (default 32768). Its installs go to `playshelf_data/sim_console/` (`source/sim/sim_install.cpp`), with the host tests' keys: it installs the packages `tests/install/packages.hpp` builds, not a console's |
| `PLAYSHELF_SIM_GITHUB_LOGIN=ok` | The GitHub device flow of *GitHub account*: `ok` approves at once, `denied` refuses, `offline` fails |
| `PLAYSHELF_SIM_HBLOADER=1` | A homebrew loader that can hand an update over to a store |
| `PLAYSHELF_SIM_HOLD_LOADING=1` | Not the console: the start-up stops on the loading screen, at its last step (the `loading` smoke scenario's screenshot) |
| `PLAYSHELF_SIM_LATEST=1.1.0:24.0.0` | The latest release and the newest firmware it supports, for the update check (`offline` for no network) |
| `PLAYSHELF_SIM_NOW=<POSIX seconds>` | A frozen console time (with `TZ=` for its time zone) |
| `PLAYSHELF_SIM_NO_CFW=1` | No Atmosphère |
| `PLAYSHELF_SIM_NO_GAME=1` | No game open (applet mode with nothing behind it) |
| `PLAYSHELF_SIM_NO_PLAY=1` | No play time on the console (`sim/sim_play.cpp`; unset, each game's play time, starts and last play are made up from its title id) |
| `PLAYSHELF_SIM_PASTE=https://gist.github.com/u/ab12` | What GitHub answers to *Send a report…* (`offline` for no network) |
| `PLAYSHELF_SIM_REGION=2` | The console region |
| `PLAYSHELF_SIM_SIGNED=1` | The packages signed with the host tests' key (the web simulator's demo server's, `tools/web_demo.cpp`) taken as signed by Nintendo: installed without the question an unsigned content asks |
| `PLAYSHELF_SIM_SPEED=96` | The web simulator only: how fast its demo library server answers, in KB/s (96 unless set; `0`: at once), so a download is seen going (`source/sim/web_http.cpp`) |
| `PLAYSHELF_SIM_STATS=<file>` | Not the console: one line per frame (frame time with the FPS limiter off; nanovg's draw calls and triangles with `tools/perf_nanovg_stats.patch`), see `tools/perf_frames.sh`. On the console (and the desktop), *About › Help & diagnostics › Developer › Frame statistics* writes one line per second to `logs/frame_stats.txt` instead: the mode (docked, handheld), the UI thread's own time per frame and the late frames (`source/kit/action/frame_stats_flow.cpp`) |
| `PLAYSHELF_SIM_STILL_FOCUS=1` | Not the console: the focus highlight without its moving glow, the loading screen's ring still, every movement of the look drawn at its end (`effects::motion()`: no entrance, no pulse, no slide), and the footer's hints never greyed while a screen or a dialog opens or closes (borealis patch `0014`), for screenshots that compare |
| `PLAYSHELF_SIM_STORE=1.1.0` | The version of PlayShelf the Homebrew App Store's catalogue has (`none`: not listed, `offline` for no network): with its record on the simulated SD card (`playshelf_data/sd/switch/appstore/.get/packages/PlayShelf/manifest.install`), an update through a store first checks the catalogue has the new version |
| `PLAYSHELF_SIM_VERSION=1.0.0:1a2b3c4` | Not the console: the version and commit the header's badge shows (`1.0.0:` for a release), not the build's own, for screenshots that compare |

Run it on a desktop with a D-Bus session (`dbus-run-session -- build-desktop/playshelf`); its data folder is `playshelf_data/` in the working directory, the simulated SD card `playshelf_data/sd/`.
