# Troubleshooting

What to do when a catalogue cannot be read, a server is not found, a download fails or a game installed does not start, and how to send a report.

## The catalogue cannot be read

*The catalogue could not be read*, with the reason:

- **the server's certificate was not trusted**: a server of your network with a certificate of its own is trusted once its key is compared, from the question PlayShelf asks; a server whose key changed must be forgotten first. See [Certificates](servers.md#certificates).
- **the index is not JSON**, **the server's answer is not a catalogue**: the address is not the server's API nor a JSON index. Check it in a browser, and set *Catalogue* in the server's profile to the format it speaks. See [What PlayShelf reads](servers.md#what-playshelf-reads).
- A user name or password refused: change them in the server's profile.

**Read again**, under the catalogue, reads it from the server once more.

## No server found on the network

*Find on this network* lists the servers that answer a discovery request on UDP port 8465 (AeroFoil and Ownfoil do). Nothing found:

- the server is off, or on another network than the console (a guest Wi-Fi, a different subnet);
- a firewall on the server's computer blocks UDP port 8465;
- the server does not answer discovery: add it by hand with *Add a server* and its `https://` address.

## A download fails

The package says why, with what to do: see [When a download stops](downloads.md#when-a-download-stops).

## A game installed does not start

- sys-patch or sigpatches are missing, or older than the console's firmware: update them.
- An update or an add-on needs a newer firmware than the console's.
- The base game is missing: an update or an add-on alone does not start. The game's sheet installs what is missing.

## Names or icons are missing

A game known only by its file name takes its name and icon from the title database: *Preferences › Title database › Download*, then **Update now** from time to time. A source that fails says so under *Sources*, with the reason. An icon still missing comes from Nlib API when the console is online and *Missing icons from Nlib API* is on; a game it does not know is asked again only once PlayShelf restarts.

## Sending a report

*Preferences › Help & diagnostics*:

- **Export a diagnostic report** saves one to the SD card, to attach to a [bug report](https://github.com/JigSawFr/PlayShelf/issues/new). It never contains the console's serial number.
- **Send a report…** puts it in a secret gist of your GitHub account and shows its link with a QR code.

A crash is saved as a crash file, which the next start says and offers to send.

Back to the [README](../README.md).
