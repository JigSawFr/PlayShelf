#!/usr/bin/env python3
"""Makes resources/data/popular.json: how well known each Switch game is, and
whether it is talked about more these days, for Home's "Popular" and
"Trending" rows (docs/contributing/popular.md). No key, no account: Wikidata
gives the games with their Switch title ids (P11072), how many Wikipedias
have a page on each (its sitelinks) and its English page; Wikimedia's
pageviews say how often that page was read in each of the last three whole
months.

A game's score (0 to 100) weighs its sitelinks and its views of the last
month, each on a log scale against the most of all; its trend is the change
of its views last month against the two before, in percent (0 when it is
read too little to say). The views are asked for the best known games only
(VIEWED, by sitelinks), one at a time: Wikimedia's API turns many away. Its
refusals cut the views short, not the file: what was read is used, the rest
scored by their sitelinks.

Usage: tools/popular.py [out.json]   (default resources/data/popular.json)
"""
import datetime
import json
import math
import re
import sys
import time
import urllib.parse
import urllib.request

AGENT = "PlayShelf-tools/1.0 (https://github.com/JigSawFr/PlayShelf)"
SPARQL = "https://query.wikidata.org/sparql"
QUERY = """
SELECT ?tid ?links ?article WHERE {
  ?game wdt:P11072 ?tid; wikibase:sitelinks ?links.
  OPTIONAL { ?article schema:about ?game; schema:isPartOf <https://en.wikipedia.org/>. }
}
"""
VIEWS = ("https://wikimedia.org/api/rest_v1/metrics/pageviews/per-article/en.wikipedia/all-access/user/"
         "{article}/monthly/{start}/{end}")
TID = re.compile(r"^0100[0-9A-F]{9}000$")
TRENDING_VIEWS = 500   # last month's views below which the trend says nothing
VIEWED = 400           # the games whose views are asked, the most sitelinks first
PAUSE = 0.25           # seconds between two of them


def fetch(url, tries=4, patient=True):
    for attempt in range(tries):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": AGENT, "Accept": "application/json"})
            with urllib.request.urlopen(request, timeout=60) as r:
                return json.load(r)
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
            if attempt == tries - 1:
                raise
            if e.code == 429 and not patient:
                raise
            if e.code == 429:
                # Rate-limited: as long as the service says (a minute or more).
                time.sleep(max(int(e.headers.get("Retry-After", "60") or 60), 60) + 5)
                continue
        except (urllib.error.URLError, TimeoutError):
            if attempt == tries - 1:
                raise
        time.sleep(2 ** attempt)
    return None


def games():
    """{title id: (sitelinks, English article or None)}, the most sitelinks of an item's ids."""
    body = fetch(SPARQL + "?" + urllib.parse.urlencode({"query": QUERY, "format": "json"}))
    out = {}
    for b in body["results"]["bindings"]:
        tid = b["tid"]["value"].strip().upper()
        if not TID.match(tid):
            continue
        links = int(b["links"]["value"])
        article = None
        if "article" in b:
            article = urllib.parse.unquote(b["article"]["value"].rsplit("/wiki/", 1)[-1])
        old = out.get(tid)
        if not old or links > old[0] or (links == old[0] and article and not old[1]):
            out[tid] = (links, article or (old[1] if old else None))
    return out


def months(today):
    """The first days of the last three whole months, oldest first, and the end of the last."""
    first = today.replace(day=1)
    starts = []
    for _ in range(3):
        first = (first - datetime.timedelta(days=1)).replace(day=1)
        starts.insert(0, first)
    end = today.replace(day=1) - datetime.timedelta(days=1)
    return starts, end


def views(article, starts, end):
    """The article's views in each of `starts`' months (0 for none)."""
    url = VIEWS.format(article=urllib.parse.quote(article.replace(" ", "_"), safe=""),
                       start=starts[0].strftime("%Y%m%d"), end=end.strftime("%Y%m%d"))
    body = fetch(url, tries=2, patient=False)
    counts = {}
    for item in (body or {}).get("items", []):
        counts[item["timestamp"][:6]] = item["views"]
    return [counts.get(s.strftime("%Y%m"), 0) for s in starts]


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "resources/data/popular.json"
    today = datetime.date.today()
    starts, end = months(today)
    known = games()
    print(f"{len(known)} games with a Switch title id", file=sys.stderr)

    read = {}
    best = sorted(((links, a) for links, a in known.values() if a), reverse=True)[:VIEWED]
    refused = 0
    for _, article in best:
        if article in read:
            continue
        try:
            read[article] = views(article, starts, end)
        except urllib.error.HTTPError as e:
            refused += 1
            if refused >= 3:
                print(f"views: refused ({e.code}), {len(read)} read, the rest scored by sitelinks", file=sys.stderr)
                break
            time.sleep(30)
            continue
        time.sleep(PAUSE)
    print(f"{len(read)} English pages read", file=sys.stderr)

    most_links = max((links for links, _ in known.values()), default=1)
    most_views = max([c[-1] for c in read.values()] + [1])
    table = {}
    for tid, (links, article) in sorted(known.items()):
        counts = read.get(article, [0, 0, 0]) if article else [0, 0, 0]
        last = counts[-1]
        link_part = math.log1p(links) / math.log1p(most_links)
        # Read: its views weigh; not read: its sitelinks alone, a little less.
        score = 0.4 * link_part + 0.6 * math.log1p(last) / math.log1p(most_views) if article in read else 0.8 * link_part
        before = (counts[0] + counts[1]) / 2
        trend = 0
        if last >= TRENDING_VIEWS and before > 0:
            trend = max(-100, min(999, round((last - before) * 100 / before)))
        if round(score * 100) > 0:
            table[tid] = [round(score * 100), trend]

    with open(out_path, "w", encoding="utf-8") as f:
        json.dump({"generated": today.isoformat(), "month": starts[-1].strftime("%Y-%m"),
                   "source": "Wikidata (P11072, sitelinks) and Wikimedia pageviews (en.wikipedia)",
                   "games": table}, f, separators=(",", ":"), sort_keys=False)
        f.write("\n")
    print(f"{out_path}: {len(table)} games", file=sys.stderr)


if __name__ == "__main__":
    main()
