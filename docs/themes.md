# Themes

PlayShelf draws every colour from its theme: the light or dark theme, then a **palette** over it. The palette can be PlayShelf's own, one of the palettes that come with it, or a theme of your own: a file of colours, or a folder that also brings pictures (a background, a logo, the picture of a game without a cover, decorations). A change shows at once, on every screen, without restarting.

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

- Your own themes (below), by their names.

Each row shows a swatch of the palette's colours, its background with its surface as a ring and its motif in its accent (a snowflake for Winter, a pumpkin for Halloween, a wave for Ocean…), or the theme's own thumbnail when it has one.

The seasons and the feasts also have a few particles drifting gently behind the screens: snow for Winter and Christmas, petals for Spring, leaves for Autumn, embers for Halloween, confetti for New Year's Eve, hearts for Valentine's Day, gold sparkles for the Lunar New Year (none in Summer). They only move while motion is allowed: with *Preferences › Visual effects* at *Reduced motion*, or at *Automatic* with the console in hand, the palette shows without them. After a minute without a button pressed they fade out and stop, and come back with the next press.

The same list is at the end of *Preferences › Theme* and of the theme button in the header (or **−**, then *Theme*). A palette made for one theme only (Ocean, Sand, the high contrast ones) also switches PlayShelf to that theme; one made for both follows *Preferences › Theme*. Choosing the other theme then changes the palette too: to its other half when it has one (High contrast light and High contrast dark), else back to Default; the theme picker shows which beforehand. The choice is kept in [`config.json`](config.md) (`theme_file`).

## Your own themes

A theme is a small text file whose name ends in `.theme`, or, when it has pictures, a folder holding its file, named `theme.theme`, and its pictures ([below](#theme-folders-and-pictures)). Put it in the `themes` folder of PlayShelf's data folder:

- on the console: `sd:/config/playshelf/themes/` (make the folder: PlayShelf does not);
- in the desktop simulator: `playshelf_data/themes/`, next to `config.json`.

Then open *Preferences › Palette* again: the theme is listed by its `name`. A file PlayShelf cannot read at all is left out of the list; a line it cannot read is skipped. Either way, what was wrong (the file and the line) is written to the session's log (`logs/sessions/` in the data folder), with a warning when a text would be hard to read on its background.

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
- `icon = moon` is the motif its row shows in the palette list: `snowflake`, `heart`, `lantern`, `blossom`, `sun`, `wave`, `leaf`, `pumpkin`, `tree`, `firework`, `gem`, `dune`, `contrast`, `moon`, `bolt`, `sparkle` (the default), or any other of [Aurore's icons](https://github.com/JigSawFr/Aurore/blob/main/docs/themes.md#the-format).
- `background_image = picture.jpg` puts a picture behind every screen; pictures and the keys that use them are [below](#theme-folders-and-pictures).
- `@include other.inc` reads the lines of another file of the folder at that place (name it `.inc`, so that it is not listed).
- Lines starting with `#` are comments; a later line wins over an earlier one.

### PlayShelf's own colours

Besides Aurore's keys, a file can set the colours of the dial on the loading screen:

| Key | What |
|---|---|
| `playshelf/dial/line` | Its rings and minutes, drawn faint |
| `playshelf/dial/track` | The day's track |
| `playshelf/dial/teal` | Its glow and the day's arc, faint |

The loading screen otherwise takes the theme's background (or its background picture) and text. PlayShelf's logo keeps its own colours, unless the theme brings a logo of its own (below).

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

## Theme folders and pictures

A theme with pictures is a folder in `themes/`, named after the theme, holding its file `theme.theme` and its pictures (PNG or JPEG), next to it or in subfolders:

```
config/playshelf/themes/
├── mint.theme          # a single file
└── nightfall/          # a theme folder
    ├── theme.theme
    ├── night.jpg
    └── logo-dark.png
```

A picture is named relative to the theme's file and must stay in the theme's folder: an absolute path, a `..` going out of it or a link to a file out of it is refused, and the line is skipped (the session's log says which). Each key below can be written at the top, for both themes, or in `[light]` or `[dark]` for one; `none` takes away a picture the theme inherits. Without these keys, nothing changes.

**The background**

| Key | Values | What |
|---|---|---|
| `background_image` | a picture, or `none` | Behind every screen, the loading screen included |
| `background_fit` | `cover` (the default), `contain`, `tile`, `center` | `cover` fills the screen, cut at its edges; `contain` shows it whole; `tile` repeats it; `center` puts it in the middle as it is |
| `background_dim` | 0 to 1 (0.35 by default) | A veil of the theme's background colour over the picture, so the text reads: 0 shows the picture as it is |
| `surface_opacity` | 0 to 1 (1 by default) | How opaque the cards, tiles, chips, the sidebar and the sheets are over the picture: under 1, the picture shows through them a little |

A busy or bright picture wants a stronger veil (0.5 to 0.7). The session's log warns when the text would read badly on the picture under its veil.

**The logo, icons and PlayShelf's pictures**

| Key | What |
|---|---|
| `logo` | PlayShelf's mark in the header and on About, and its icon on the loading screen, fitted in their square |
| `icon.<name>` | One of the icons of the screens (`settings`, `info`, `gamepad`…: [the names](https://github.com/JigSawFr/Aurore/blob/main/docs/themes.md#logos-and-images)); `icon.<name>.tint = true` draws a white (or black) shape in the icon's colour of the moment |
| `image.playshelf/cover/placeholder` | The picture of a game without a cover ([PlayShelf's own pictures](#playshelfs-own-pictures)) |
| `preview` | The theme's thumbnail in the palette list, instead of the swatch of its colours: a square picture, 72 × 72 at least |

**Decorations**

| Key | What |
|---|---|
| `backdrop.header`, `backdrop.sidebar`, `backdrop.dialog` | A picture over the header, the sidebar, or the panel of dialogs and sheets, under their text: keep it faint |
| `decor.top`, `decor.bottom` | A strip along the top or bottom edge of the screen, behind the screens (icicles, a garland, a snowdrift): a PNG with transparency, within the header's first 30 units or the footer's last 20 |
| `<key>_fit`, `<key>_opacity`, `decor.top_height`, `decor.bottom_height` | How each one fills its area (as `background_fit`), its opacity, a strip's height |
| `particles` | `snow`, `leaves`, `petals`, `embers`, `confetti`, `sparkles`, `hearts`, `bubbles`, or `none` |
| `particles_density` | `low` (the default), `medium`, `high`, or 0 to 1 |
| `particles_color` | Up to five colours, separated by commas (from the palette by default) |
| `particles_speed`, `particles_layer` | 0.25 to 4 times as fast; `back` (the default: behind the screens, between the cards) or `front` (over them, fainter) |

Particles follow the same rules as the seasonal palettes' (above): they only move while motion is allowed, never with *Reduced motion* or in hand on *Automatic*, and stop after a minute without a button pressed. Aurore's [guide to themes](https://github.com/JigSawFr/Aurore/blob/main/docs/themes.md#theme-folders-and-pictures) has every detail.

### PlayShelf's own pictures

A theme folder (above) can replace these pictures of PlayShelf's (`image.<key> = file.png`, in the theme's folder, PNG or JPEG):

| Key | What | Without it |
|---|---|---|
| `playshelf/cover/placeholder` | What a game without a cover shows, in the Library, the Catalogue, the Home tab, the Downloads and on its page: the picture fills the cover's square (cut at its edges, centred, its corners rounded); made at most 512 × 512 | A gamepad on the cover's well |

The theme's `logo` takes the place of PlayShelf's mark in the header and on About, and of its icon on the loading screen (over the same glow).

### An example: Nightfall

A complete theme folder, a moonlit sky at night and at dawn, is in PlayShelf's repository: [`docs/examples/themes/nightfall`](https://github.com/JigSawFr/PlayShelf/tree/main/docs/examples/themes/nightfall). Its pictures are drawn by `tools/make_example_theme.py`, a few tens of KB each:

```
nightfall/
├── theme.theme
├── night.jpg, day.jpg             # the background, at night and at dawn
├── logo-dark.png, logo-light.png  # the logo, 256 × 256
├── cover.png                      # a game without a cover, 512 × 512
├── preview.png                    # its thumbnail in the palette list, 128 × 128
└── stars-night.png, stars-day.png # the dialogs' faint pattern, tiled
```

```ini
# Nightfall: the example theme folder of docs/themes.md. Its pictures are
# drawn by tools/make_example_theme.py. Copy the whole folder into
# config/playshelf/themes/ on the SD card.
name = Nightfall
author = PlayShelf
version = 1.0
description = A moonlit sky behind every screen, at night or at dawn.
icon = moon

# The picker's thumbnail, and the picture of a game without a cover
preview = preview.png
image.playshelf/cover/placeholder = cover.png

# Behind every screen: the picture fills the window, the cards let it show a little
background_fit = cover
surface_opacity = 0.88

# A faint pattern of stars on the dialogs, and a few sparkles drifting
# (never with Preferences › Visual effects at Reduced motion)
backdrop.dialog_fit = tile
backdrop.dialog_opacity = 0.45
particles = sparkles
particles_density = low

[light]
background = #f4f0fa
surface = #ffffff
text = #241f38
accent = #5c4cbe
background_image = day.jpg
background_dim = 0.5
logo = logo-light.png
backdrop.dialog = stars-day.png

[dark]
background = #0e1430
surface = #182042
text = #eef0ff
accent = #8e9cff
background_image = night.jpg
background_dim = 0.3
logo = logo-dark.png
backdrop.dialog = stars-night.png
```

To install it: copy the whole `nightfall` folder into `config/playshelf/themes/` on the SD card (`sd:/config/playshelf/themes/nightfall/theme.theme`), then choose *Nightfall* in *Preferences › Palette*. It has both themes: *Preferences › Theme* picks the night or the dawn. To start a theme of your own, copy it under another folder name, change its `name`, and replace the pictures.

## If something looks wrong

- The palette is not listed: check that the file ends in `.theme`, is in `config/playshelf/themes/` on the SD card, and see the session's log for what PlayShelf could not read.
- A theme folder is not listed: its file must be named `theme.theme`, right inside the folder; a picture it names that is not there leaves it out too (the log says which).
- A picture does not show: it must be a PNG or a JPEG (not WebP) inside the theme's folder, named as it is written (letters' case included).
- No particles: they only move with *Preferences › Visual effects* at *Always full*, or at *Automatic* on the TV, and pause after a minute without a button pressed.
- A text is hard to read: the log says which colour on which background, and by how much; change one of the two.
- To go back to PlayShelf's own colours: *Preferences › Palette › Default*.
