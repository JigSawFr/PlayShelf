// atlas_contract — do PlayShelf's own readers still accept what Atlas publishes?
//
// Usage: atlas_contract <dir>, <dir> laid out like Atlas' data branch (platforms/switch/*.json and
// changes.json at its root; tools/atlas_contract.py fetches it from jsDelivr, which is where
// PlayShelf reads it). Each file goes through the reader the app uses and one line comes out:
//
//   file | ok, rejected or missing | what it holds
//
// Exit status: 1 when a file that is there is one its reader refuses (Atlas changed a format
// without moving `schema`, or the file is broken), or when popular.json or games.json is missing
// or holds far fewer games than they ever did; 0 otherwise. A file not published yet (the newest
// ones) is only said, never failed.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "library/also.hpp"
#include "library/changes.hpp"
#include "library/popular.hpp"
#include "library/reach.hpp"
#include "library/signals.hpp"
#include "library/upcoming.hpp"

using namespace library;

namespace
{
// A far smaller book than any real one is a truncated publish, not a game that left.
constexpr size_t MIN_GAMES = 1000;

bool slurp(const std::string& path, std::string* out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::stringstream s;
    s << in.rdbuf();
    *out = s.str();
    return true;
}

int g_failed = 0;

void say(const char* file, const char* state, const std::string& what)
{
    std::printf("%-34s | %-8s | %s\n", file, state, what.c_str());
}

// Reads `file` into `book`; `required`: it must be there with MIN_GAMES at least. `what` describes the book.
template <typename Book, typename What>
bool check(const std::string& dir, const char* file, Book& book, bool required, What what)
{
    std::string text;
    if (!slurp(dir + "/" + file, &text)) {
        say(file, "missing", required ? "REQUIRED" : "not published (yet)");
        if (required) g_failed++;
        return false;
    }
    if (!book.from_json(text)) {
        say(file, "rejected", "a reader of PlayShelf refuses it (schema or shape changed?)");
        g_failed++;
        return false;
    }
    say(file, "ok", what(book));
    if (required && book.size() < MIN_GAMES) {
        std::printf("  -> only %zu games: a truncated file?\n", book.size());
        g_failed++;
    }
    return true;
}

std::string counted(size_t n, const std::string& day)
{
    return std::to_string(n) + " games, made " + day;
}
}   // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: atlas_contract <dir>\n");
        return 2;
    }
    const std::string dir = argv[1], sw = dir + "/platforms/switch";
    std::printf("%-34s | %-8s | %s\n", "file", "state", "what PlayShelf reads of it");

    PopularBook popular;
    SignalBook  games;
    SimilarBook similar;
    EditionBook editions;
    UpcomingBook upcoming;
    ReachBook   reach;
    AlsoBook    also;
    ChangeBook  changes;
    check(sw, "popular.json", popular, true, [](const PopularBook& b) { return counted(b.size(), b.generated()); });
    if (check(sw, "games.json", games, true, [](const SignalBook& b) { return counted(b.size(), b.generated()); })) {
        size_t themes = 0, art = 0, cover = 0, series = 0, rated = 0;
        for (const auto& [id, g] : games.all()) {
            themes += !g.themes.empty();
            art += !g.igdb_art.empty();
            cover += !g.igdb_cover.empty();
            series += !g.series.empty();
            rated += g.igdb.count > 0;
        }
        std::printf("  games.json fields: %zu with themes, %zu with art, %zu with a cover, %zu in a series, %zu rated by IGDB players\n",
                    themes, art, cover, series, rated);
        // The Atlas data the new rows and lines need: say it plainly when IGDB's never arrived.
        if (!themes || !art || !cover) std::printf("  NOTE: IGDB's themes, art or covers are absent: the lines and pictures they feed stay empty\n");
    }
    check(sw, "similar.json", similar, true, [](const SimilarBook& b) { return counted(b.size(), b.generated()); });
    check(sw, "editions.json", editions, false, [](const EditionBook& b) { return counted(b.size(), b.generated()); });
    check(sw, "upcoming.json", upcoming, false, [](const UpcomingBook& b) { return counted(b.size(), b.generated()); });
    check(sw, "reach.json", reach, false, [](const ReachBook& b) {
        return counted(b.size(), b.generated()) + ", " + std::to_string(b.languages().size()) + " languages";
    });
    check(sw, "also.json", also, false, [](const AlsoBook& b) { return counted(b.size(), b.generated()); });
    check(dir, "changes.json", changes, false, [](const ChangeBook& b) { return std::to_string(b.size()) + " entries, made " + b.generated(); });
    std::printf("\n%s\n", g_failed ? "CONTRACT BROKEN" : "contract holds");
    return g_failed ? 1 : 0;
}
