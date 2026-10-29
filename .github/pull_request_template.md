<!--
The title must be a conventional commit with a scope (feat(library): …, fix(install): …,
docs(build): …): CI checks it, and the squash merge makes it the commit subject that
release-please writes the changelog from.
-->

## What this changes, and why

<!-- The diff says what; this says what it is for. Link the issue it closes, if any. -->

## How it was verified

<!-- Beyond CI. Say whether it ran on a console (firmware, Atmosphère, launcher) or only in `make desktop`. -->

Screens: none

<!-- The screens this change alters on purpose, compared with main's by CI's visual check (tests/visual/README.md):
`Screens: smoke/01_library, loading/*` (scenario/name patterns), `any` for every screen, `none` for no change meant.
A screen that changes without being named here fails the desktop job. -->

## Checklist

- [ ] `make check` passes
- [ ] A screen that changes on purpose is named on the `Screens:` line above
- [ ] New strings are in every `resources/i18n/*/playshelf.json` (English is fine as a placeholder)
- [ ] New website texts are in every `site/i18n/*.json`, translated
- [ ] `source/kit/` includes nothing of the app (`tools/check_sources.py`)
- [ ] Nothing sent or stored in the clear: TLS verified, no secret in a plain file
