# Reference screens

What `tools/desktop_smoke.py` shows on screens that come out the same in every
run (console time fixed, footer clock redrawn at the console's time, 16:00),
one folder per scenario: `smoke/` (the default one) and `loading/`.

Each folder's `SCREENS` names the stable screens compared, one a line: that
list is all git keeps. The references are **main's own screens**, never
files kept by hand (`tools/visual_refs.py`): each run of main keeps its
scenarios' screens, masked, in `.visual-refs/<scenario>/`, in the Actions
cache for the next runs (`visual-refs-…`) and as the run's
`…-refs` artifact. A pull request restores main's and compares with them
(`tools/visual_check.py`); a scenario without any (a cache gone, a new
scenario) compares nothing yet, said as a warning.

CI compares each new run with them and uploads the side-by-side images of
any screen that changed. **A changed screen fails the build unless the pull
request announces it** on the `Screens:` line of its description (the
template has it): `Screens: smoke/01_library, smoke/0*` (`scenario/name`
patterns), `Screens: any` for every screen, `Screens: none` or no line when
no screen is meant to change. Once the pull request is merged, main's run
keeps the new screens: nothing to update. The smoke test draws the focus
without its moving glow (`PLAYSHELF_SIM_STILL_FOCUS`) and the header's
version badge on one made-up development build (`PLAYSHELF_SIM_VERSION`), so
a screen nobody changed comes out the same. The screens are drawn by the
runner image's software renderer (mesa's llvmpipe on `ubuntu-26.04`, the
image `.github/workflows/build.yml` pins), so a new image can move every
screen a little: moving to one is a pull request announcing `Screens: any`.

Adding a screen: its name in the scenario's `SCREENS`; the first run says
*new* (nothing to compare with yet), main's next run keeps it. Keep out
screens with a toast (`smoke/08_dev_enabled`), timings or other values that
change from one run to the next (`smoke/10_dev_report`, the start-up's
times), and anything that piles up between local runs.

Locally, the same check against references of your own:

```sh
make desktop
xvfb-run -s "-screen 0 1280x720x24" python3 tools/desktop_smoke.py smoke
python3 tools/visual_refs.py keep smoke smoke       # this run's screens as the references
# … change something, build and run the smoke test again, then:
python3 tools/visual_check.py smoke .visual-refs/smoke --screens tests/visual/smoke/SCREENS --scenario smoke
```

`python3 tools/visual_refs.py fetch --run <run id>` (with `gh` signed in)
takes a run of main's references instead; `--expected "smoke/01_library"` or
`--expected any` does what the `Screens:` line would.
