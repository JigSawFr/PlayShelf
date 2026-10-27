# Themes

PlayShelf draws every colour from its theme: the light or dark theme, then a **palette** over it. The palette can be PlayShelf's own, one of the palettes that come with it, or a theme file of your own. A change shows at once, on every screen, without restarting.

## Choosing a palette

*Preferences › Palette* lists:

- **Default**: PlayShelf's own colours, navy or cool white.
- The palettes that come with PlayShelf:

  | Palette | Theme | What it looks like |
  |---|---|---|
  | High contrast dark | dark | White on black, a yellow focus ring, thicker |
  | High contrast light | light | Black on white, a deep blue focus ring, thicker |
  | Ocean | dark | Deep blue, a sky-blue accent |
  | Amethyst | light and dark | Violet, following the theme chosen |
  | Sand | light | Warm paper, a terracotta accent |
  | Winter, Valentine's Day, Lunar New Year, Spring, Summer, Autumn, Halloween, Christmas, New Year's Eve | light and dark | One for each season and feast: frost and silver, rose, red lacquer and gold, cherry blossoms, sea and sunset, maple, pumpkin on a violet night, pine and gold, midnight and champagne |
  | Seasonal | light and dark | The one of the day, by the calendar: Halloween from 17 October, Christmas in December, and so on. It changes by itself: the day a season or a feast begins, the screens take its palette at midnight, or as soon as the console wakes up. The seasons are the northern hemisphere's; Aurore's [theme guide](https://github.com/JigSawFr/Aurore/blob/main/docs/themes.md#the-seasons) has the exact days |

- Your own theme files (below), by their names.

The same list is at the end of *Preferences › Theme* and of the theme button in the header (or **−**, then *Theme*). A palette made for one theme only (Ocean, Sand, the high contrast ones) also switches PlayShelf to that theme; one made for both follows *Preferences › Theme*. Choosing the other theme then changes the palette too: to its other half when it has one (High contrast light and High contrast dark), else back to Default; the theme picker shows which beforehand. The choice is kept in [`config.json`](config.md) (`theme_file`).

## Your own theme files

A theme file is a small text file whose name ends in `.theme`. Put it in the `themes` folder of PlayShelf's data folder:

- on the console: `sd:/config/playshelf/themes/` (make the folder: PlayShelf does not);
- in the desktop simulator: `playshelf_data/themes/`, next to `config.json`.

Then open *Preferences › Palette* again: the file is listed by its `name`. A file PlayShelf cannot read at all is left out of the list; a line it cannot read is skipped. Either way, what was wrong (the file and the line) is written to the session's log (`logs/sessions/` in the data folder), with a warning when a text would be hard to read on its background.

### The shortest one

Three lines: a name, the theme it starts from, and an accent colour. PlayShelf works out the rest (the focus ring, the switches, the buttons, the chosen tab), keeping the text readable.

```ini
name = Mint
inherit = dark
accent = #3ddc97
```

### What a file can say

```ini
# Shown in the list
name = Dusk
author = You

# What it starts from: light, dark, one of the palettes above
# (builtin:ocean, builtin:sand, builtin:amethyst, builtin:high-contrast-dark,
# builtin:high-contrast-light) or another file of the folder. Dark if absent.
inherit = dark

# A few colours, from which every other one is worked out
background = #1b1626
surface = #251e33
text = #f1ecf8
accent = #ff8fab
warning = #ffc861
danger = #ff6b6b
```

- Colours are written `#rgb`, `#rrggbb`, `#rrggbbaa`, `rgb(r, g, b)` or `rgba(r, g, b, a)` (`a` from 0 to 1).
- `[light]` and `[dark]` sections hold what is for one theme only: with both, the file follows *Preferences › Theme*.
- `[colors]` sets any single colour by its key (the list is in [Aurore's guide to themes](https://github.com/JigSawFr/Aurore/blob/main/docs/themes.md#every-key)), such as `cadran/text3` for the quietest text or `brls/sidebar/background` for the sidebar.
- `counterpart = other.theme` (or a palette, `builtin:ocean`), in a file made for one theme only: the file with the other theme, the same look in light or dark. Choosing the other theme switches to it.
- `background_image = picture.png` puts a picture, next to the file, behind every screen.
- `@include other.inc` reads the lines of another file of the folder at that place (name it `.inc`, so that it is not listed).
- Lines starting with `#` are comments; a later line wins over an earlier one.

### PlayShelf's own colours

Besides Aurore's keys, a file can set the colours of the dial on the loading screen:

| Key | What |
|---|---|
| `playshelf/dial/line` | Its rings and minutes, drawn faint |
| `playshelf/dial/track` | The day's track |
| `playshelf/dial/teal` | Its glow and the day's arc, faint |

The loading screen otherwise takes the theme's background and text. PlayShelf's logo keeps its own colours.

### Two themes in one file

This one is light on the light theme and dark on the dark theme:

```ini
name = Forest
author = You

[light]
background = #f1f4ee
surface = #ffffff
text = #1d2a1f
accent = #2f7d4a
playshelf/dial/line = #1d2a1f

[dark]
background = #121a14
surface = #1a251c
text = #e6efe7
accent = #6fcf8f
playshelf/dial/teal = #6fcf8f
```

## If something looks wrong

- The palette is not listed: check that the file ends in `.theme`, is in `config/playshelf/themes/` on the SD card, and see the session's log for what PlayShelf could not read.
- A text is hard to read: the log says which colour on which background, and by how much; change one of the two.
- To go back to PlayShelf's own colours: *Preferences › Palette › Default*.
