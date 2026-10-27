# Popular games

Part of the [contributor's guide](../../CONTRIBUTING.md).

Home's **Popular on your servers** and **Trending** rows read `resources/data/popular.json`: for each Switch game (by its base game's title id), a score from 0 to 100 and a trend. No service PlayShelf could ask without a key ranks Switch games, so the file is made ahead, from two open sources, by `tools/popular.py`:

- **Wikidata** lists the games with a Nintendo Switch title id ([P11072](https://www.wikidata.org/wiki/Property:P11072)), how many Wikipedias have a page on each (its sitelinks: how well known it is the world over) and its English page;
- **Wikimedia's pageviews** say how often that page was read in each of the last three whole months, for the best known games (`VIEWED`, 400 by default, one request at a time: the API turns many away).

A game's score weighs its sitelinks (40 %) and its views of the last month (60 %), each on a log scale against the most of all; a game whose views were not read is scored by its sitelinks alone (80 %). Its trend is the change of its views last month against the two months before, in percent, when it was read at least 500 times. Home shows only the games of the servers it read: a score of 20 or more is popular, and popular with a trend of +25 % or more is trending.

```sh
tools/popular.py            # writes resources/data/popular.json
```

It takes a few minutes. When Wikimedia refuses the views (HTTP 429), what was read is kept and the rest is scored by its sitelinks: run it again later for a better file. The file goes in a pull request like any change; PlayShelf comes with the one of its build and, with *Preferences › Title database › Popular games from Wikipedia* on ([`popular_online`](../config.md)), fetches the one on `main` once a day through jsDelivr (`https://cdn.jsdelivr.net/gh/JigSawFr/PlayShelf@main/resources/data/popular.json`), so a new file reaches every console without a release. The newer of the two (its `generated` day) is used.

The file:

```json
{"generated": "2026-10-27", "month": "2026-09", "source": "...",
 "games": {"010025400AECE000": [93, 12], "0100B0C013912000": [71, -40]}}
```

`source/library/popular.cpp` reads it (`tests/library_popular/`): a base game's id only, 20,000 games at most, scores and trends bounded.
