# File formats

Part of the [contributor's guide](../../CONTRIBUTING.md).

Every file one version of PlayShelf writes and another reads (an older one after going back, a newer one after an update, the simulator, another program) carries a whole number, its **schema**:

- Adding a field keeps the schema; a field that changes meaning, unit or type, or goes away, takes the next one.
- A reader skips the fields it does not know, and treats a newer schema as no file at all (the defaults, or the copy it had), never as an error. A file written before it had a schema counts as schema 1.
- A file of a newer schema is never written over: PlayShelf works in memory and leaves it as it is.
- A file PlayShelf writes again keeps the keys it does not know, so going back to an earlier build loses nothing.
- A file published to installed versions keeps its old schema published while they read it (`compat.json`).
- Each reader has its tests: no schema, a newer one (ignored, the file left intact), an unknown field (kept when written again).

The schema is one named constant per format, never a number repeated in the code. Paths are under `sd:/config/playshelf/` (`./playshelf_data/` for the desktop simulator).

| File | Schema | Its constant | Read and written by | Newer schema |
|---|---|---|---|---|
| `config.json` | `"schema"`, 1 | `config::SCHEMA` (`web/simulator.js`'s `CONFIG_SCHEMA` the same, `tools/check_resources.py` checks it) | `source/util/config.cpp`, `web/simulator.js` ([its keys](../config.md)) | Defaults in memory, the file not written |
| `servers.json` | `"schema"`, 1 | `library::SERVERS_SCHEMA` | `source/library/servers.cpp` | No servers, the file not written; a password or pin of a format not known kept as it is, unused |
| `queue.json`, with each job's install journal | `"schema"`, 1 (the journal's own `"schema"`, 1) | `download::QUEUE_SCHEMA`, `download::JOURNAL_SCHEMA` | `source/download/queue.cpp` | An empty queue, the file not written; a job of a newer journal not taken up (failed, its journal kept) |
| `seen.json` | `"schema"`, 1 | `SCHEMA` in `source/library/seen.cpp` | `source/library/seen.cpp`, `source/action/insights.cpp` | Not read, not written |
| `news.json` (the games the bell told new or trending, from Atlas' `changes.json`) | `"schema"`, 1 | `library::TellBook::SCHEMA` | `source/library/news.cpp`, `source/activity/insight_notices.cpp` | Read as none (a game may be told again), not written |
| `nlib/facts.json` | `"schema"`, 1 | `SCHEMA` in `source/library/facts.cpp` | `source/library/facts.cpp`, `source/action/nlib_flow.cpp` | Not read, not written |
| `wishlist.txt`, `not_interested.txt` | a first line `schema=1` (none: 1) | `library::IDS_SCHEMA` (`source/kit/util/text_schema.hpp` reads the line) | `source/library/seen.cpp`, `source/action/wishlist.cpp`, `source/action/not_interested.cpp` | Empty, not written |
| `notices_read.txt` | a first line `schema=1` (none: 1) | `notice_logic::KEYS_SCHEMA` | `source/action/notice_logic.cpp`, `source/action/notices.cpp` | Empty, not written |
| `sd:/config/boreale/apps/playshelf.conf` (shared with the other Boréale homebrews) | a line `schema=1` (none: 1) | `BL_APP_SCHEMA` (boreale-link) | `source/action/shared_bell_logic.cpp` (PlayShelf's own, at each start when something changed); the others read it | Read, never written over; unknown keys kept |
| `sd:/config/boreale/notices/playshelf.json`, and the other homebrews' (shared bell) | `"schema"`, 1 | `BL_NOTICES_SCHEMA` (boreale-link) | `source/action/shared_bell_logic.cpp`: PlayShelf writes its own when its bell changes (each entry's route: the tab where it is dealt with, which `--open=<tab>` opens) | No notices from it; its own never written over; unknown members kept |
| `previous_build.json`, `boot_check.json`, `went_back.json` | `"schema"`, 1 | `rollback::SCHEMA` | `source/kit/util/rollback.cpp` | Read whatever the schema: the build gone back to is older by design, so a key is only ever added |
| `migrations.json` | `"schema"`, 1, and its `level` | `migrations::SCHEMA` | `source/kit/util/migrations.cpp` | A later build's level set back to this one's, so that build runs its steps again |
| `github_token` | a `v1:` prefix (`util/secret_box`) | `secret_box`'s prefix | `source/kit/util/github_auth.cpp` | Signed out, the file left as it is |
| `cache/dev_builds.json` | `"schema"`, 1 (`"version"` before, still read) | `dev_builds::CACHE_SCHEMA` | `source/kit/util/dev_builds.cpp` | Fetched again (a cache) |
| `titledb/<hash>/state.json` | `"schema"`, 1 | `titledb::STATE_SCHEMA` | `source/titledb/store.cpp` | Fetched again (a cache) |
| `titledb/<hash>/titles-*.pack`, `icons-*.pack` | CyberFoil-DB's magic and version (`CFTITLE1`, `CFICONP1`) | `source/titledb/pack.cpp` | `source/titledb/pack.cpp`; both opened before `state.json` names them | Refused, the packs in use kept |
| `cache/*.json` of [Atlas](https://github.com/JigSawFr/Atlas) (`popular.json`, `games.json`, `similar.json`, `editions.json`, `prices/<COUNTRY>.json`, `index.json`), and `resources/data/`'s copies (refreshed by `tools/atlas_copies.py`, which takes only Atlas' schema 1 for the Switch, and a `games.json` with titledb's names) | `"schema"`, 1 | `library::ATLAS_SCHEMA` | `source/library/popular.cpp`, `signals.cpp`, `prices.cpp`, `insights_index.cpp` | Refused, the copy PlayShelf had kept |
| `compat.json`, `compat-1.json` (each release's assets) | `"schema"`, 1 | `update::COMPAT_SCHEMA` (`tools/gen_compat.py`'s `SCHEMA` the same, checked when it runs) | written by `tools/gen_compat.py`, read by `source/kit/util/update.cpp` | A build fetches `compat-<its schema>.json`, then `compat.json`, which stays at schema 1 for the builds that read only it |
| The web simulator's kept state (IndexedDB) | one database per deployment, named after the page's folder | `SimLogic.keptDatabase` | `web/simulator.js` | Each deployment its own: one never reads another's files |
| boreale.dev's `versions.json` (the website's menu) | flat, or `"schema"`, 1 with `"projects"` | `VERSIONS_SCHEMA` in `tools/build_site.py` | `tools/build_site.py` (`versions_of()`) | No versions in the menu |

Formats that are not ours (a library server's answers, Tinfoil indexes, CyberFoil-DB's manifest, Nlib API, GitHub's) carry no schema of ours: they are read with the same care, an unknown field skipped and an unexpected shape read as nothing.
