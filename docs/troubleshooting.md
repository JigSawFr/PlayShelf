# Troubleshooting

What to do when a catalogue cannot be read, a server is not found, a download fails or a game installed does not start, and how to send a report.

## The catalogue cannot be read

*The catalogue could not be read*, with the reason:

- **this console is not paired with the server**: pair it again, from the question PlayShelf asks or the server's profile. See [Pairing](servers.md#pairing).
- **refused (the server's key is not the one pinned)**: the server was reinstalled or given another certificate. Forget the pairing in the server's profile and pair again, after comparing the new key. See [Certificates](servers.md#certificates).
- **another server than the one that was paired**: the address now answers with another server; check it, or pair again.
- **the server does not speak Lantern**: the address is not a Lantern server's. A server of another kind needs a [bridge](https://github.com/JigSawFr/lantern-bridge).

**Read again**, under the catalogue, reads it from the server once more.

## No server found on the network

*Find on this network* lists the servers that answer a discovery request on UDP port 47115 (Lantern servers do). Nothing found:

- the server is off, or on another network than the console (a guest Wi-Fi, a different subnet);
- a firewall on the server's computer blocks UDP port 47115;
- the server does not answer discovery: add it by hand with *Add a server* and its `https://` address.

## A download fails

The package says why, with what to do: see [When a download stops](downloads.md#when-a-download-stops).

## A game installed does not start

- sys-patch or sigpatches are missing, or older than the console's firmware: update them.
- An update or an add-on needs a newer firmware than the console's.
- The base game is missing: an update or an add-on alone does not start. The game's page installs what is missing.

## Names or icons are missing

A game known only by its file name takes its name and icon from the title database: *Preferences › Title database › Download*, then **Update now** from time to time. A source that fails says so under *Sources*, with the reason. An icon still missing comes from Nlib API when the console is online and *Icons and game pages from Nlib API* is on; a game it does not know is asked again only once PlayShelf restarts.

## Sending a report

*About › Help & diagnostics*:

- **Export a diagnostic report** saves one to the SD card, to attach to a [bug report](https://github.com/JigSawFr/PlayShelf/issues/new). It never contains the console's serial number.
- **Send a report…** puts it in a secret gist of your GitHub account and shows its link with a QR code.
- With no network or account at all, the report can leave the console as photographs: in developer mode, *Show the diagnostic report*, then **R** shows it as QR codes (four to a page, the start of the log left out if they would be too many). Photograph every code, in any order, and run `python3 tools/decode_report.py photo1.jpg photo2.jpg …` (needs `pip install zxing-cpp pillow zstandard`) to put the report back together; attach the file it writes to the bug report.

A crash is saved as a crash file, which the next start says and offers to send.

Back to the [README](../README.md).
