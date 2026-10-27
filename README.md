<div align="center">

<img src=".github/readme/banner.png" alt="PlayShelf" width="100%">

### Your game library, on the console

**HTTPS everywhere · Pause and resume · Nothing stored in the clear**

</div>

---

PlayShelf is a Nintendo Switch homebrew that browses a library server you run yourself (an [AeroFoil](https://github.com/luketanti/AeroFoil) or [Ownfoil](https://github.com/a1ex4/ownfoil) server, or a plain JSON index) and installs **your own backups and homebrew** from it. With an AeroFoil server, it also installs its cheats, keeps copies of your saves on it and asks it for a game.

It is built on the same technical base and the same *Cadran* look as [PlayGuard](https://github.com/JigSawFr/PlayGuard).

**[Website](https://jigsawfr.github.io/PlayShelf/)** · **[Try it in your browser](https://jigsawfr.github.io/PlayShelf/simulator/)** · **[Documentation](docs/getting-started.md)**

> [!WARNING]
> **Early development.** Nothing is released yet: the catalogue, the downloads and the title database work in the [web simulator](https://jigsawfr.github.io/PlayShelf/simulator/), and are still to be checked on a console.

| Catalogue | Downloads |
|---|---|
| ![The Catalogue](images/screenshots/catalogue.png) | ![The Downloads](images/screenshots/downloads.png) |

> [!IMPORTANT]
> PlayShelf installs content you own. It does not support, and will not support, encrypted Tinfoil indexes, Tinfoil's authentication headers or any service that distributes games without the rights to do so.

## Build

```sh
git clone --recursive https://github.com/JigSawFr/PlayShelf && cd PlayShelf
make test        # the host tests
make desktop     # the real UI against a simulated console
make dist        # playshelf.zip for the SD card (devkitPro)
```

Everything else: [CONTRIBUTING.md](CONTRIBUTING.md).

## License

**GPLv3**, see [`LICENSE`](LICENSE). Maintained by [JigSawFr](https://github.com/JigSawFr).

<sub>PlayShelf is not affiliated with or endorsed by Nintendo. Nintendo Switch is a trademark of Nintendo.</sub>
