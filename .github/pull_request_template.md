<!--
The title must be a conventional commit with a scope (feat(library): …, fix(install): …,
docs(build): …): CI checks it, and the squash merge makes it the commit subject that
release-please writes the changelog from.
-->

## ✨ At a glance

<!-- One or two sentences: what changes for the player, or for the code, and whether it is a fix, a feature or a refactor. -->

## 🤔 Why

<!-- The reason, and the issue it closes, if any. The diff says what; this says what it is for. -->

## 🔧 What changes

<!-- The main parts of the change. Skip what the diff already makes obvious. -->

## 🖼️ Before / After

<!-- For each screen named on the `Screens:` line: a picture from `tools/desktop_smoke.py`, or from the console for a firmware-dependent change. Drop the table when no screen changes. -->

| Screen | Before | After |
|---|---|---|
| | | |

Screens: none

<!-- The screens this change alters on purpose, compared with main's by CI's visual check (tests/visual/README.md):
`Screens: smoke/01_library, loading/*` (scenario/name patterns), `any` for every screen, `none` for no change meant.
A screen that changes without being named here fails the desktop job. -->

## ✅ Checked

<!-- Beyond CI, which runs `make test`, the resource, source, web-parameter and compatibility checks, the visual check, clang-tidy and CodeQL. Say whether it ran on a console (firmware, Atmosphère, launcher) or only in `make desktop`. -->

- [ ] Nothing sent or stored in the clear: TLS verified, no secret in a plain file
