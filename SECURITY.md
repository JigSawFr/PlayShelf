# Security policy

## Reporting a vulnerability

Report privately through GitHub, from the repository's **Security** tab:
[Report a vulnerability](https://github.com/JigSawFr/PlayShelf/security/advisories/new).
That opens a private advisory visible only to you and the maintainer. Please do not open a
public issue: PlayShelf holds the addresses and credentials of servers people run at home.

Expect an acknowledgement within a few days. A confirmed report is fixed in a new release,
and the advisory is published once that release is out.

Include, if you can: the firmware and Atmosphère versions, the PlayShelf version, the kind
of server (AeroFoil, Ownfoil, a JSON index), what a person gains, and the smallest sequence
of steps that shows it.

## Supported versions

The latest release. Fixes are not backported. While no release is out, `main`.

## What is in scope

- **Credentials** — a server's password, token or address reaching a log, the diagnostic
  report, a backup or anything written in the clear on the SD card.
- **TLS** — a connection that skips certificate verification, or accepts another certificate
  than the one pinned for a local server.
- **What a server sends** — a malformed index, section, image or file making PlayShelf
  crash, write outside the folders it installs to, or install something other than what was
  chosen.
- **Installs** — a package written to the wrong place, or a damaged package left installed
  after a failed or cancelled download.
- **Updates** — anything that makes PlayShelf trust an update that did not come from this
  repository's releases.

## What is not

- **The server itself.** Securing the library server, its accounts and the network it is on
  is its owner's part.
- **Physical access to the SD card.** Whoever can edit the card can already change
  PlayShelf's settings or replace the homebrew.
- **Custom firmware itself.** PlayShelf needs Atmosphère; a modded console can run any
  homebrew.
- **Content without the rights to it.** PlayShelf installs what its user owns. It does not
  support, and will not support, encrypted Tinfoil indexes, Tinfoil's authentication headers
  or any service that distributes games without the rights to do so.
- **Nintendo's software.** Behaviour of the console's system belongs with Nintendo. PlayShelf
  is not affiliated with Nintendo.
