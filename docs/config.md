# `config.json`

PlayShelf keeps its settings in `sd:/config/playshelf/config.json` (`./playshelf_data/config.json` for the desktop simulator). Every key is optional: a missing one, or one of the wrong type, keeps its default, and a value out of range goes back to its default, one key at a time. A file that cannot be read at all is kept aside as `config.json.bad` (the previous one as `config.json.bad.1`) and the defaults are used. The file is written whole each time, through `config.json.tmp`.

| Key | Values | Default | What it sets |
|---|---|---|---|
| `schema` | `1` | `1` | The layout of this file |
| `language` | `system` or a language code (`en-US`, `fr`, `fr-CA`, `de`, `es`, `es-419`, `it`, `nl`, `pt`, `pt-BR`, `ru`, `ja`, `ko`, `zh-Hans`, `zh-Hant`) | `system` | *Preferences › Language*: the console's own, or one forced |
| `theme` | `system`, `light`, `dark` | `dark` | *Preferences › Theme* |
| `text_size` | `auto`, `normal`, `large` | `auto` | *Preferences › Text size*: `auto` is larger while docked |
| `effects` | `auto`, `full`, `reduced` | `auto` | *Preferences › Visual effects*: `auto` keeps the lighter effects off the dock |
| `intro` | `true`, `false` | `true` | The loading screen stays 2 s at start-up |
| `start_tab` | `library`, `catalogue`, `preferences`, `about` | `library` | *Preferences › Start on* |
| `support_reminder` | `true`, `false` | `true` | "Support PlayShelf" once a month at start-up |
| `keep_awake` | `true`, `false` | `true` | *Downloads › Stay awake while downloading*: the console does not go to sleep by itself while downloads run |
| `connections` | `1` to `8` | `4` | *Downloads › Connections per download*: how many requests a download makes at once, each for a 4 MB piece of the package taken in turn; more help with a server that serves each connection slowly. The packages are still installed one after the other |
| `prefetch` | `0` to `2` | `0` | *Downloads › Fetch ahead*: while a package installs, the next ones (as many) are fetched into `cache/prefetch/` when the SD card has room for them and their install (1 GiB to spare); their install starts from the file, the rest from the server. A file goes once its package is installed or removed |
| `support_reminded` | `YYYY-MM-DD` | none | The day of the last reminder |
| `seen_version` | a version | none | The version whose "What's new" was shown |
| `crash_seen` | a crash file's name | none | The newest crash file already said |
| `update_daily` | `true`, `false` | `false` | *Preferences › Check for updates every day* |
| `update_via` | `inplace`, `auto`, `sphaira`, `appstore`, `manual` | none | *Preferences › Update with* (none: asked at the first update) |
| `update_checked` | `YYYY-MM-DD` | none | The day of the last check |
| `update_latest` | a version | none | The latest release the last check found |
| `update_snooze_version` | a version | none | The release *Later* was chosen for on the update screen… |
| `update_snooze_until` | `YYYY-MM-DD` | none | …and until when (30 days at most) |
| `dev_mode` | `true`, `false` | `false` | The developer tools (seven presses of Ⓨ on *About › Version*) |
| `frame_stats` | `true`, `false` | `false` | *Developer › Frame statistics* |
| `cadran_glow` | `true`, `false` | `true` | *Developer › Background glow* |
| `library_view` | `grid`, `list` | `grid` | *Library*, Ⓨ: the games as covers or as a list |
| `dev_builds_order` | `newest`, `number` | `newest` | The order of the pull requests in *Install another build* |
| `logs_keep` | `0`, `5`, `10`, `20`, `50` | `20` | The reports and crash files kept, by number (`0`: no limit) |
| `logs_keep_days` | `0`, `7`, `30`, `90`, `365` | `0` | …by age in days |
| `logs_keep_mb` | `0`, `1`, `5`, `20` | `0` | …by size in MB |

The GitHub sign-in of the developer tools is kept apart, in `sd:/config/playshelf/github_token`, sealed for this console: encrypted (AES-128-CTR) and authenticated (HMAC-SHA256) with keys derived from the console's own key (`source/kit/util/secret_box.hpp`), so a copy of the SD card opens nothing on another console. A token an older version kept as it is is sealed the first time it is read.
