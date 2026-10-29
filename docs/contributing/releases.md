# Pull requests and releases

Part of the [contributor's guide](../../CONTRIBUTING.md).

Releases are automated with [release-please](https://github.com/googleapis/release-please), which reads [conventional commits](https://www.conventionalcommits.org/):

- Give each pull request a conventional title with a scope, the part of the app it changes (`feat(library): …`, `fix(install): …`, `docs(build): …`, `feat(remote)!: …` for a breaking change); CI checks it.
- **Squash-merge** pull requests, so each one lands as a single commit carrying that title.
- release-please keeps a `chore(main): release X.Y.Z` pull request open; merging it tags the release and the build attaches `playshelf.nro`, `playshelf.zip`, `compat.json` and `SHA256SUMS.txt` to it. Before 1.0.0, a feature bumps the minor version.
- release-please runs with a token of a GitHub App (variable `RELEASE_APP_ID`, secret `RELEASE_APP_PRIVATE_KEY`; permissions Contents, Pull requests and Issues: write), not `GITHUB_TOKEN`, so its pushes run the required checks and it can create the tag.

## Development builds

Nothing is published for them. The developer tools install the `playshelf-<version>-<ref>-<commit>-nro` artifact of a build run: main's last commits and the newest run of each open pull request from a branch of this repository (forks are left out, their code not reviewed yet). GitHub hands artifacts to signed-in users only, hence the GitHub sign-in (device flow; the GitHub App's client ID in `source/kit/util/github_auth.hpp`). Every build but a release one (`PLAYSHELF_RELEASE=1`) shows itself as a development build in the header: an amber tag with the version and the commit.

## Reproducible zips

`make dist` gives every file in the zip the date `SOURCE_DATE_EPOCH` (default: the last commit's time) and adds the files in a fixed order without extra attributes, so the same commit and `.nro` give a byte-identical zip. The deko3d shaders are part of the build: Aurore compiles nanovg's GLSL with uam (devkitPro's `uam` package, in `switch-dev`) into `build/aurore/dksh/` and builds them into the library, so the `.nro` carries them and its RomFS has none; configure stops when uam is missing. uam gives the same bytes for the same GLSL from one run to the next, so two builds of the same commit with the same toolchain image give the same `.nro` and zip.
