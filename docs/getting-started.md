# Getting started

PlayShelf browses a library server you run yourself and installs your own backups and homebrew from it: a game's base, its updates and its add-ons, as NSP, NSZ, XCI or XCZ, over HTTPS. This page goes from the SD card to a first game installed.

> [!WARNING]
> **Early development.** No release is published yet. The [web simulator](https://jigsawfr.github.io/PlayShelf/simulator/) shows where PlayShelf stands, against a demo server of made-up games.

## What you need

- A Nintendo Switch with [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere).
- [sys-patch](https://github.com/impeeza/sys-patch) or sigpatches, for the games installed to start. PlayShelf looks for them before it installs, and asks whether to go on when it finds neither.
- A library server of your own, reachable over HTTPS: [AeroFoil](https://github.com/luketanti/AeroFoil), Ownfoil, or any web server with a JSON index of your files. See [Library servers](servers.md).

## Install PlayShelf

1. Download `playshelf.zip` from the [latest release](https://github.com/JigSawFr/PlayShelf/releases).
2. Extract it to the root of the SD card: it puts `switch/playshelf/playshelf.nro` in place.
3. Start PlayShelf from the homebrew menu. Started over a game (hold R while starting one), it has the console's full memory; from the album it runs as an applet, with less.

PlayShelf keeps its settings and its data in `sd:/config/playshelf/`: its servers, its downloads, its title database and its reports.

## Add your server

The first start opens on the **Library**, which says there is no server yet.

- **Add a server**: its name, its `https://` address on your network, over the Internet or both, and its user name and password if it has an account. See [Adding a server](servers.md#adding-a-server).
- **Find on this network**: lists the AeroFoil and Ownfoil servers that answer on the console's network, to pick one.

## Install a game

The **Catalogue** shows the server's games as covers, with their size and their add-ons. **Search** and **Order** narrow it; a server with sections shows them too.

| Catalogue | A game's page |
|---|---|
| ![The Catalogue](../images/screenshots/catalogue.png) | ![A game's page](../images/screenshots/game_sheet.png) |

Press Ⓐ on a game for its page: **Install everything**, or its base game, one of its updates or one of its add-ons. Each one goes to the **Downloads**. When the console is online, the page also shows what [Nlib API](https://api.nlib.cc/nx/) knows of the game in PlayShelf's language: its banner, publisher, year and categories, a summary, its screenshots (Ⓐ shows one on the whole screen, ◀ ▶ the others) and its description. The Library opens the same page for a game on the console.

## Follow the downloads

The **Downloads** install the packages one after the other, each with its progress, its speed and the time left. Ⓐ on a game or a package offers to pause it, resume it, try it again or remove it. Once installed, the game is in the **Library**. See [Downloads](downloads.md).

| Downloads | Library |
|---|---|
| ![The Downloads](../images/screenshots/downloads.png) | ![The Library](../images/screenshots/library.png) |

## Names and icons

A game that neither the console nor the server names takes its name and icon from the title database, kept on the SD card: *Preferences › Title database › Download*. Its sources are CyberFoil-DB, NXMeta and Ghostland by default, and you can add your own. See [`title_sources`](config.md).

An icon that none of them has is asked of [Nlib API](https://api.nlib.cc/nx/) over the Internet, for the games on screen only, then kept in the cache. Base games only: an update or an add-on shows its game's icon. *Preferences › Title database › Icons and game pages from Nlib API* turns it off ([`nlib_api`](config.md)).

## Preferences

*Preferences* sets the language (the console's, or one of 15), the theme and its palette (PlayShelf's, a preset or [your own](themes.md)), the text size, the tab PlayShelf starts on, the updates and the title database. Every setting is also in [`config.json`](config.md).

![The Preferences](../images/screenshots/preferences.png)

Back to the [README](../README.md).
