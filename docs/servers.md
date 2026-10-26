# Library servers

PlayShelf installs from a library server you run yourself, on a computer or a NAS: it reads the server's catalogue, then downloads the packages from it. Up to 16 servers can be kept, each with its own address and account.

## What PlayShelf reads

- **AeroFoil and Ownfoil**, through their API (`/api/remote`, else `/api/shop`): the server's sections, its games with their sizes, versions and icons, and its message of the day.
- **A JSON index** on any web server: an object with `files`, `directories`, `titledb` or `success`.

*Catalogue*, in a server's profile, is **Found out** by default: PlayShelf tries the API first, then reads the address as an index. Set it to the one your server speaks to skip the guess.

PlayShelf does not read encrypted Tinfoil indexes, and sends neither Tinfoil's authentication headers nor any identifier of the console.

### A JSON index

```json
{
  "success": "Welcome to my shelf",
  "files": [
    { "url": "games/Copper Canyon Rally [0100DE0000020000][v0].nsp", "size": 104857600 },
    "games/Copper Canyon Rally [0100DE0000020800][v65536].nsp"
  ],
  "directories": [ "dlc/index.json" ]
}
```

- `files`: each package, as its address or an object with `url` (or `path`, `file`, `download_url`), and optionally `name`, `size`, `title_id`, `version`, `app_type` and `icon_url`. An address is relative to the index's own.
- Without a `title_id`, the title id and the version come from the tags of the name or the file name: `[0100DE0000020000][v65536]`.
- `directories`: more indexes, read in turn.
- `titledb`: names for the title ids, `{ "0100DE0000020000": { "name": "Copper Canyon Rally" } }`, for the packages known only by their file name.
- `success`: a message shown above the catalogue.

## Adding a server

*Library › Add a server*, or in the *Catalogue*, the **Server** row:

- **Name**: as the Catalogue shows it.
- **On this network** and **Over the Internet**: the server's `https://` addresses, one of them at least. Only `https://`: the catalogue and the packages must come over a verified connection.
- **Address used**: **Automatic** uses the network address when the server answers there, else the Internet one; or always one of them.
- **User name** and **Password**, when the server asks for them (HTTP Basic, over HTTPS only). The password is sealed for this console (encrypted and authenticated with keys derived from the console's own), so a copy of the SD card opens nothing elsewhere.

**Find on this network** sends a discovery request on the console's network (UDP port 8465, as AeroFoil and Ownfoil answer it) and lists the servers that answer, to add one with its address filled in.

## Certificates

Every connection is HTTPS with its certificate verified; no setting turns the check off.

- A server with a certificate from a known authority (Let's Encrypt, say) is trusted as it is.
- A server **on your network** with a certificate of its own: the first connection shows the fingerprint of its key. Compare it with the one the server shows; **Trust this server** keeps it, and the server is trusted with that key alone from then on (pinned).
- Reinstalled, or given a new certificate, the server shows another key: PlayShelf refuses it. Forget the old key in the server's profile (**Trusted key**, Ⓐ), then trust the new one.

## Where the servers are kept

`sd:/config/playshelf/servers.json`, their passwords sealed in it. Deleting a server deletes its password and its trusted key.

Back to the [README](../README.md).
