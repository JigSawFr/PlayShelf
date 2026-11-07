# Library servers

PlayShelf installs from a library server you run yourself, on a computer or a NAS, that speaks [Lantern](https://lantern.boreale.dev): it reads the server's catalogue, then downloads the packages from it, each one checked against the size and the SHA-256 the server announced. Up to 16 servers can be kept, each with its own address.

Lantern is the only protocol PlayShelf speaks. A server that speaks another one (AeroFoil, Ownfoil, a JSON index, Tinfoil's) needs a bridge that serves it as Lantern: [lantern-bridge](https://github.com/JigSawFr/lantern-bridge).

PlayShelf sends no identifier of the console to a server.

## Adding a server

**Add a server**, on *Home* and in the *Catalogue* while there is no server yet, then from the *Catalogue*'s **Server** pill:

- **Name**: as the Catalogue shows it.
- **On this network** and **Over the Internet**: the server's `https://` addresses, one of them at least. Only `https://`: the catalogue and the packages must come over a verified connection.
- **Address used**: **Automatic** uses the network address when the server answers there, else the Internet one; or always one of them.
- **Pairing**: whether this console is paired with the server. **Save** pairs a new server at once.

**Find on this network** sends a discovery request on the console's network (UDP port 47115) and lists the Lantern servers that answer, to add one with its address filled in.

## Pairing

A server lets a console in once a person has agreed on the server's side:

1. PlayShelf asks the server for a code and shows it, with a QR code.
2. On the server's page (the address shown), enter the code, or scan the QR code with a phone, and sign in there to let this console in.
3. PlayShelf is told, and keeps what the server gave it: a token, bound to a key this console made for this server only. Every request is signed with that key, so a copy of the token alone opens nothing.

The token and the key are kept in `sd:/config/playshelf/lantern.json`, sealed for this console (encrypted and authenticated with keys derived from the console's own): a copy of the SD card opens nothing elsewhere. *Forget the pairing*, in the server's profile, or deleting the server, erases them; the server's own list of devices is yours to tidy on its side.

## What a server offers

The catalogue is the server's sections and its games, with their sizes and versions, and its message of the day. When the server keeps title sheets, PlayShelf asks for those of the first 300 games: their publisher, date, genres and icon fill what Nlib API does not know (a game it has no page for, or no network), and the icon shows in *Downloads*. A server offers more or less than that, and PlayShelf shows what it offers. Once its catalogue is read, its **Server** pill in the *Catalogue* has up to three more lines, and a game's sheet up to two (in the *Catalogue*, and in the *Library* for a game of that server):

- **Cheats**: the server's cheats, by game and build. **Install** writes a build's file where Atmosphère looks for it, `sd:/atmosphere/contents/<title id>/cheats/<build id>.txt`; **Install every build** writes them all, and Atmosphère loads the one that matches the game's version. A build's codes can be shown, and a file removed.
- **Save backups**: a user's saves (chosen first when the console has several) and the server's copies of them. **Back up this save now** sends the game's save, with a note to tell it apart; **Put back on this console** replaces the save with a copy. The save it replaces is kept on the SD card first, in `sd:/config/playshelf/saves/<title id>/`, and put back if anything goes wrong. A game must have been played once on the console to have a save to put one in.
- **Requests**: the games you asked the server for, and how each stands; **Ask for a game** finds one by its name in the title databases, or by its title id.

What a person may do depends on the scopes the server gave this console when it paired.

## Homebrew

The *Homebrew* tab lists the homebrew a server offers for the console (SPEC § 8): its name, version and author, and whether the console has it or an update. **Install** fetches the package whole, checks its size and SHA-256 against what the server announced, then writes it where the Switch profile says: a single `.nro` at `sd:/switch/<id>/<id>.nro`, an overlay in `sd:/switch/.overlays/`, an archive unpacked into its own folder. Every file of an archive is checked against the package's manifest before any is moved into place. A package that writes outside its own folders (under `sd:/atmosphere/`, for example) shows each path first and installs only if you agree. An **update** of a single file reuses what the copy on the SD card already holds: when the package offers a chunk index (SPEC § 22), only the chunks that differ are downloaded, each checked, and the whole file is checked at the end (a server without an index, or an index that does not match, gives the whole file as before). When the server offers peers (SPEC § 19), the console first asks, once a minute at most, the other paired consoles of the network whether one already holds the package: what a peer gives is checked chunk by chunk and as a whole like any download, with a grant that names this one file and nothing of the server's credentials, and a peer that does not hold it or fails is passed by (see below for the other way round). An **update** also removes the files the new version no longer ships; **Remove** deletes what the homebrew wrote and the folders it left empty. A **port** (an engine rebuilt to run a game, SPEC § 8.6) needs files from your own copy of the game: the port's menu has **Check game data**, which looks for each required file where the entry says (under `sd:/switch/<port>/`, say), reads it, and tells which are missing, each with where to find it in your copy, or whether a file was only found and not verified. Nothing of the game is ever downloaded. An entry whose data folder lies outside the places the Switch profile allows for ports is not listed. What is installed is kept in `sd:/config/playshelf/homebrew.json`. With *Downloads › Share homebrew with the network* on (it is off until you turn it on), this console also gives the homebrew files it installed whole from a server to the other paired consoles of the same private network (SPEC § 19.4): it answers their searches for peers, and serves a file only to a request that carries a short-lived grant from the server it names, for that file alone, and only to addresses of your own network, one download at a time. It never serves a save, a key, a game or a setting. What it holds is listed in `sd:/config/playshelf/peers.json`.

## Downloads

A package is installed as it arrives: the installer asks the server for the ranges it needs, each a signed request of the SDK's client, so the install goes on while the package downloads and takes no room beyond itself. The contents are checked by the installer as it reads them; the whole file's SHA-256 is checked only for a package fetched ahead, which the client writes to the SD card (a download cut short resumes where it stopped, the file hashed again first) and which is installed from that file once whole.

## Certificates

Every connection is HTTPS with its certificate verified; no setting turns the check off.

- A server with a certificate from a known authority (Let's Encrypt, say) is trusted as it is.
- A server **on your network** with a certificate of its own: pairing shows the fingerprint of its key, next to the code. Compare it with the one the server shows; once paired, the server is trusted with that key alone (pinned).
- Reinstalled, or given a new certificate, the server shows another key: PlayShelf refuses it. Forget the pairing in the server's profile and pair again, after comparing the new key.

## Where the servers are kept

`sd:/config/playshelf/servers.json`: each server's name, addresses and `uid`. Its credentials are in `lantern.json`, by that `uid`.

Back to the [README](../README.md).
