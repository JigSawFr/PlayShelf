# Downloads

A game's page in the *Catalogue* adds its packages to the **Downloads**: its base game, its updates and its add-ons, each a package. They are installed one after the other, straight from the server to the console: nothing is first saved whole to the SD card.

## The queue

Each game shows its packages, with their progress, their speed and the time left; the sidebar's **Downloads** shows how many are left and where they are, from any tab.

- Ⓐ on a game or a package: **Pause** or **Resume** it, **Try again**, or **Remove** it (what it installed so far is deleted).
- The queue's own actions: **Pause all**, **Resume all**, **Clear installed**.
- A package already on the console is not installed again.

The queue is kept on the SD card (`sd:/config/playshelf/queue.json`). A download cut short (PlayShelf closed, the console turned off) is paused at the next start, and its install goes on from where it stopped once resumed.

## When a download stops

A connection lost or a server that cannot answer for now: PlayShelf tries again by itself three times, from where it stopped, then the package says why it failed and what to do:

| Says | What to do |
|---|---|
| connection lost, server unavailable | **Try again** once the server is back. A server with two addresses: **Use the server's other address**. |
| access refused | The user name or the password: **Edit the server**. |
| no longer on the server | The package was renamed or moved: **Look for it in the catalogue** finds it again. |
| server key not trusted | The server shows another key: forget the old one in its profile, then trust it again. See [Certificates](servers.md#certificates). |
| not enough free space | Free some space on the SD card, then **Try again**. |
| damaged package | A content of the package is not what it should be: the file on the server may be damaged. |

A content that is not signed by Nintendo stops its package: it may have been changed or damaged. **Install anyway** goes on, at your own risk.

## Settings

Under the queue, and in [`config.json`](config.md):

- **Stay awake while downloading**: the console does not go to sleep by itself while downloads run (on by default).
- **Connections per download**, 1 to 8 (4 by default): how many requests a download makes at once, each for a 4 MB piece of the package. More help with a server that serves each connection slowly.
- **Fetch ahead**: while a package installs, the next one or two are fetched to the SD card, when it has room for them and their install; their install then starts from the file. Each takes its size twice on the SD card until it is installed (off by default).

## Before the first install

Games installed this way need sys-patch or sigpatches to start. When PlayShelf finds neither on the console, it says so before installing and asks whether to go on.

Back to the [README](../README.md).
