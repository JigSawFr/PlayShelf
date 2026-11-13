// The simulators' library server (web/README.md): a catalogue of made-up
// games, their packages and icons, written to <out-dir>: packaged with the
// web simulator at /app/samples/demo, read by the desktop one from
// build-desktop/demo, and served by source/sim/sim_demo_http.cpp as
// https://demo.playshelf.example. The packages are built as the install
// suites build theirs (tests/install/packages.hpp: the test keys, which the
// simulated console takes), small: what they install is a stand-in. Built
// and run on the host by `make web` and `make desktop` (it needs libzstd and
// zlib).
//
// With them, what Home reads of Atlas about those games, made up as
// well (insights/platforms/switch/: popular.json, games.json with each game's name and themes, similar.json,
// editions.json, upcoming.json, reach.json and also.json, in Atlas' schema 1 and its platforms/switch/ folder), which sim_demo_http.cpp answers in place of
// Atlas' own, and the console's first games (console/meta/: a content
// meta record for each, empty; console/control/: its name and icon), which
// PlayShelf puts on the simulated console at a first start with the demo
// (source/sim/sim_demo_seed.cpp): so Home shows its rows, the recommendations
// and their reasons among them.
//
//   web_demo <out-dir>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <aurore/extern/nlohmann/json.hpp>

#include <lantern/chunks.hpp>
#include "install/packages.hpp"
#include "util/sha256.hpp"
#include "library/seen.hpp"
#include "util/text_schema.hpp"

using namespace install;
using namespace test_install;

namespace
{
// A game of the table (tools/web_demo_games.json).
struct Game
{
    std::string name;
    std::string date;              // its release, YYYY-MM-DD
    int         updates = 0;       // newest first: 2 → v131072 and v65536
    int         dlcs = 0;
    bool        compressed = false;   // an .nsz
    bool        on_server = true;     // false: only the title database knows it
    bool        installed = false;    // on the console at a first start
    bool        wished = false;       // on its wishlist at a first start
    // What Atlas would say of it: its series ("" for none), its
    // studio, its genres, how well known it is, how fast that grows and the
    // critics' score (popular.json), its Steam reviews (0 and 0: not on Steam),
    // its IGDB players' rating (0 and 0: none) and the game it is an edition
    // of ("" for none: editions.json).
    std::string              series, studio;
    std::vector<std::string> genres;
    int                      score = 0, trend = 0, critics = 0;
    int                      positive = 0, negative = 0;
    int                      igdb = 0, igdb_count = 0;
    std::string              edition_of;
};

// `day` (YYYY-MM-DD) and `days` after it.
std::string day_after(const std::string& day, int days)
{
    std::tm t{};
    t.tm_year = std::stoi(day.substr(0, 4)) - 1900;
    t.tm_mon = std::stoi(day.substr(5, 2)) - 1;
    t.tm_mday = std::stoi(day.substr(8, 2)) + days;
    t.tm_hour = 12;
    const std::time_t when = timegm(&t);
    char              out[16];
    std::strftime(out, sizeof out, "%Y-%m-%d", std::gmtime(&when));
    return out;
}

// The table's games, their `days` made dates from `today`.
std::vector<Game> read_games(const std::string& path, const std::string& today)
{
    std::ifstream        in(path);
    const nlohmann::json j = nlohmann::json::parse(in);
    std::vector<Game>    out;
    for (const nlohmann::json& g : j.at("games")) {
        Game game;
        game.name = g.at("name").get<std::string>();
        game.date = g.contains("days") ? day_after(today, g.at("days").get<int>()) : g.at("date").get<std::string>();
        game.updates = g.value("updates", 0);
        game.dlcs = g.value("dlcs", 0);
        game.compressed = g.value("compressed", false);
        game.on_server = g.value("on_server", true);
        game.installed = g.value("installed", false);
        game.wished = g.value("wished", false);
        game.series = g.value("series", "");
        game.studio = g.at("studio").get<std::string>();
        game.genres = g.at("genres").get<std::vector<std::string>>();
        game.score = g.value("score", 0);
        game.trend = g.value("trend", 0);
        game.critics = g.value("critics", 0);
        if (g.contains("steam")) {
            game.positive = g["steam"].at(0).get<int>();
            game.negative = g["steam"].at(1).get<int>();
        }
        if (g.contains("igdb")) {
            game.igdb = g["igdb"].at(0).get<int>();
            game.igdb_count = g["igdb"].at(1).get<int>();
        }
        game.edition_of = g.value("edition_of", "");
        out.push_back(std::move(game));
    }
    return out;
}

// The title id of the table's game `index` (from 0): 0100DE00000(index + 1)0000.
uint64_t id_of_game(size_t index)
{
    return 0x0100DE0000000000ull | ((uint64_t)(index + 1) << 16);
}

uint8_t s_next_id = 0x10;   // the content ids: each its own byte, all of them distinct

ContentInfo add(const Bytes& nca, ContentType type)
{
    return info_of(nca, type, s_next_id++);
}

// A package of `type` for `title`: a program (an update's: smaller) and a
// control, or an add-on's data.
Bytes package(MetaType type, uint64_t title, uint32_t version, size_t body, const std::string& seed, bool compressed)
{
    Title t;
    t.meta_id = id_of(s_next_id++);
    std::vector<ContentInfo> infos;
    std::vector<File>        files;
    if (type == MetaType::AddOnContent) {
        const Bytes data = content_nca(4, body, seed.c_str());
        const auto  info = add(data, ContentType::Data);
        infos.push_back(info);
        files.push_back({ content_id_hex(info.id) + ".nca", data });
    } else {
        t.program = content_nca(0, body, (seed + " program").c_str());
        t.control = content_nca(2, 0x900, (seed + " control").c_str());
        t.program_info = add(t.program, ContentType::Program);
        t.control_info = add(t.control, ContentType::Control);
        infos = { t.program_info, t.control_info };
        const std::string p = content_id_hex(t.program_info.id);
        files.push_back(compressed ? File{ p + ".ncz", ncz_of(t.program) } : File{ p + ".nca", t.program });
        files.push_back({ content_id_hex(t.control_info.id) + ".nca", t.control });
    }
    t.cnmt = packaged_cnmt(type, title, version, infos);
    // An add-on's application: its own, not the suites' one.
    if (type == MetaType::AddOnContent) put_le64(t.cnmt.data() + 0x20, (title & ~0xfffull) - 0x1000);
    else if (type == MetaType::Application) put_le32(t.cnmt.data() + 0x20 + 8, 0x0c000000);
    t.meta_nca = cnmt_nca(t.cnmt);
    t.files = { { content_id_hex(t.meta_id) + ".cnmt.nca", t.meta_nca } };
    t.files.insert(t.files.end(), files.begin(), files.end());
    t.files.push_back({ "01000000000100000000000000000003.tik", stream(0x2c0, "ticket") });
    t.files.push_back({ "01000000000100000000000000000003.cert", stream(0x700, "cert") });
    return nsp(t);
}

std::string json_text(const std::string& s)
{
    std::string out = "\"";
    for (char c : s) out += c == '"' ? std::string("\\\"") : std::string(1, c);
    return out + "\"";
}

void write(const std::string& path, const std::string& bytes)
{
    std::ofstream(path, std::ios::binary) << bytes;
}
// ------------------------------------------------------ Atlas' files

// A game's traits of each kind: [id, label] pairs. The ids are made up too
// (Q and a number from 9 000 000), one for each label of a kind.
using Traits = std::map<std::string, std::vector<std::string>>;   // kind: labels

Traits traits_of(const Game& g)
{
    Traits t;
    if (!g.series.empty()) t["series"] = { g.series };
    t["developers"] = { g.studio };
    t["publishers"] = { g.studio };
    t["genres"] = g.genres;
    t["modes"] = { "single-player video game" };
    for (const std::string& genre : g.genres)
        if (genre == "party" || genre == "racing" || genre == "fighting" || genre == "shooter") {
            t["modes"].push_back("multiplayer video game");
            break;
        }
    return t;
}

std::string trait_id(const std::string& kind, const std::string& label)
{
    static std::map<std::string, int> ids;
    const auto it = ids.emplace(kind + "\n" + label, 9000000 + (int)ids.size()).first;
    return "Q" + std::to_string(it->second);
}

// How much sharing a trait of a kind counts, as Atlas weighs it
// (its tools/features.py): a series most, a mode least.
double kind_weight(const std::string& kind)
{
    return kind == "series" ? 4.0 : kind == "developers" ? 2.0 : kind == "genres" ? 1.0 : 0.5;
}

// Each game's most similar ones, 1..100, as Atlas finds them: a
// cosine over the traits, each weighed by its kind and how rare it is.
std::vector<std::vector<std::pair<size_t, int>>> similar_of(const std::vector<Traits>& all)
{
    std::map<std::string, int> count;   // "kind\nlabel": games with it
    for (const Traits& t : all)
        for (const auto& [kind, labels] : t)
            for (const std::string& l : labels) count[kind + "\n" + l]++;
    auto weight = [&](const std::string& kind, const std::string& l) {
        return kind_weight(kind) * std::log(1.0 + (double)all.size() / count[kind + "\n" + l]);
    };
    std::vector<double> norm(all.size());
    for (size_t i = 0; i < all.size(); i++) {
        double n = 0;
        for (const auto& [kind, labels] : all[i])
            for (const std::string& l : labels) n += weight(kind, l) * weight(kind, l);
        norm[i] = std::sqrt(n);
    }
    std::vector<std::vector<std::pair<size_t, int>>> out(all.size());
    for (size_t i = 0; i < all.size(); i++) {
        std::vector<std::pair<double, size_t>> ranked;
        for (size_t j = 0; j < all.size(); j++) {
            if (j == i) continue;
            double dot = 0;
            bool   strong = false;   // a publisher or a mode alone is not enough
            for (const auto& [kind, labels] : all[i]) {
                const auto other = all[j].find(kind);
                if (other == all[j].end()) continue;
                for (const std::string& l : labels)
                    if (std::find(other->second.begin(), other->second.end(), l) != other->second.end()) {
                        dot += weight(kind, l) * weight(kind, l);
                        if (kind != "publishers" && kind != "modes") strong = true;
                    }
            }
            const double cosine = dot / (norm[i] * norm[j]);
            if (strong && std::lround(cosine * 100) > 0) ranked.push_back({ -cosine, j });
        }
        std::sort(ranked.begin(), ranked.end());
        for (size_t k = 0; k < ranked.size() && k < 12; k++) out[i].push_back({ ranked[k].second, (int)std::lround(-ranked[k].first * 100) });
    }
    return out;
}

std::string hex_id(uint64_t id)
{
    char b[17];
    std::snprintf(b, sizeof b, "%016llX", (unsigned long long)id);
    return b;
}

// popular.json, games.json, similar.json, editions.json, upcoming.json, reach.json
// and also.json of the table's games, made on `day` (today: newer than the files
// PlayShelf ships, which it takes in their place). Made up like the rest: the
// reach, the other stores and the IGDB themes follow from a game's place in the
// table, only to give every row and line something to show.
void write_insights(const std::string& dir, const std::vector<Game>& table, const std::string& day)
{
    mkdir(dir.c_str(), 0755);
    const std::string head = "{\"schema\": 1, \"generated\": \"" + day + "\", \"source\": \"made up for the demo\", ";
    std::string popular = head + "\"month\": \"" + day.substr(0, 7) + "\", \"views\": true, \"games\": {";
    std::string games = head + "\"games\": {";
    std::string similar = head + "\"games\": {";
    std::string editions = head + "\"games\": {";
    std::string reach = head + "\"languages\": [\"en\", \"fr\", \"de\", \"es\", \"it\", \"ja\", \"nl\", \"pt\", \"ko\", \"ru\", \"zh\"], \"games\": {";
    std::string also = head + "\"platform\": \"switch\", \"games\": {";
    std::string upcoming = head + "\"games\": {";
    std::vector<Traits> all;
    for (const Game& g : table) all.push_back(traits_of(g));
    const auto like = similar_of(all);
    for (size_t i = 0; i < table.size(); i++) {
        const Game&       g = table[i];
        const std::string sep = i ? ", " : "", key = json_text(hex_id(id_of_game(i)));
        popular += sep + key + ": [" + std::to_string(g.score) + ", " + std::to_string(g.trend) + ", " + std::to_string(g.critics) + "]";
        games += sep + key + ": {\"name\": " + json_text(g.name);
        bool first = false;
        for (const auto& [kind, labels] : all[i]) {
            games += std::string(first ? "" : ", ") + json_text(kind) + ": [";
            for (size_t l = 0; l < labels.size(); l++)
                games += std::string(l ? ", " : "") + "[" + json_text(trait_id(kind, labels[l])) + ", " + json_text(labels[l]) + "]";
            games += "]";
            first = false;
        }
        if (g.positive)
            games += ", \"steam\": {\"app\": " + std::to_string(9000000 + i) + ", \"positive\": " + std::to_string(g.positive) +
                     ", \"negative\": " + std::to_string(g.negative) + ", \"owners\": " + std::to_string(g.positive * 20) +
                     ", \"ccu\": " + std::to_string(g.positive / 50) + "}";
        if (g.igdb) games += ", \"igdb_rating\": [" + std::to_string(g.igdb) + ", " + std::to_string(g.igdb_count) + "]";
        // Two IGDB themes by the game's place: games sharing one read "Like X · Fantasy".
        {
            static const char* THEMES[] = { "Fantasy", "Survival", "Open world", "Sci-fi", "Comedy", "Stealth", "Horror", "Drama" };
            const size_t       a = i % 8, b = (i / 3 + 3) % 8 == a ? (a + 1) % 8 : (i / 3 + 3) % 8;
            games += ", \"themes\": [[\"" + std::to_string(a + 1) + "\", \"" + THEMES[a] + "\"], [\"" + std::to_string(b + 1) +
                     "\", \"" + THEMES[b] + "\"]]";
        }
        games += "}";
        // Where the game is read (the best known ones only): French, Japanese and English shares.
        if (g.score >= 40) {
            const int fr = 15 + (int)((i * 13) % 50), ja = (int)((i * 7) % 30), en = std::max(0, 100 - fr - ja);
            reach += std::string(reach.back() == '[' || reach.back() == '{' ? "" : ", ") + key + ": [" + std::to_string(en) + ", " +
                     std::to_string(fr) + ", 0, 0, 0, " + std::to_string(ja) + ", 0, 0, 0, 0, 0]";
        }
        // The other stores: Steam for a game with Steam reviews, others by its place.
        {
            std::string stores;
            auto        add = [&stores](const char* store, const std::string& id) {
                stores += std::string(stores.empty() ? "" : ", ") + "\"" + store + "\": [\"" + id + "\"]";
            };
            if (g.positive) add("steam", std::to_string(900000 + i));
            if (g.score >= 50 && i % 3 == 0) add("xbox", "9DEMO" + std::to_string(100000 + i));
            if (g.score >= 60 && i % 4 == 1) add("ps", std::to_string(10900000 + i));
            if (i % 5 == 2) add("gog", "demo_game_" + std::to_string(i));
            if (!stores.empty()) also += std::string(also.back() == '{' ? "" : ", ") + key + ": {" + stores + "}";
        }
        // Still to come: with the follows IGDB would count.
        if (g.date >= day)
            upcoming += std::string(upcoming.back() == '{' ? "" : ", ") + key + ": [" + g.date.substr(0, 4) + g.date.substr(5, 2) +
                        g.date.substr(8, 2) + ", " + json_text(g.name) + ", " + std::to_string(g.score * 60) + "]";
        for (size_t k = 0; k < table.size() && !g.edition_of.empty(); k++)
            if (table[k].name == g.edition_of)
                editions += std::string(editions.back() == '{' ? "" : ", ") + key + ": " + json_text(hex_id(id_of_game(k)));
        similar += sep + key + ": [";
        for (size_t k = 0; k < like[i].size(); k++)
            similar += std::string(k ? ", " : "") + "[" + json_text(hex_id(id_of_game(like[i][k].first))) + ", " + std::to_string(like[i][k].second) + "]";
        similar += "]";
    }
    write(dir + "/popular.json", popular + "}}\n");
    write(dir + "/games.json", games + "}}\n");
    write(dir + "/similar.json", similar + "}}\n");
    write(dir + "/editions.json", editions + "}}\n");
    write(dir + "/reach.json", reach + "}}\n");
    write(dir + "/also.json", also + "}}\n");
    write(dir + "/upcoming.json", upcoming + "}}\n");
}
}   // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: web_demo <out-dir> <games.json>\n");
        return 2;
    }
    const std::string out = argv[1];
    // The day of the build: Atlas' files made on it (newer than the
    // ones PlayShelf ships), the table's `days` counted from it.
    // PLAYSHELF_DEMO_DAY (YYYY-MM-DD) fixes it, for screenshots to compare.
    const std::time_t now = std::time(nullptr);
    char              day[16];
    std::strftime(day, sizeof day, "%Y-%m-%d", std::gmtime(&now));
    if (const char* fixed = std::getenv("PLAYSHELF_DEMO_DAY"); fixed && std::strlen(fixed) == 10) std::snprintf(day, sizeof day, "%s", fixed);
    const std::vector<Game> games = read_games(argv[2], day);
    mkdir(out.c_str(), 0755);
    mkdir((out + "/files").c_str(), 0755);
    // The demo as a Lantern server's apps (SPEC.md § 6.1), one page: each
    // package with the SHA-256 of the file as it is served.
    std::string items;
    auto        item = [&items, &out](const std::string& file, size_t size, uint64_t id, uint64_t title, uint32_t version, const char* type,
                                 const std::string& name) {
        std::string bytes;
        {
            std::ifstream in(out + "/" + file, std::ios::binary);
            bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        Sha256 sha;
        sha.update(std::string_view(bytes));
        char line[1536];
        std::snprintf(line, sizeof line,
                      "%s\n    {\"id\": \"%016llX-%u\", \"platform\": \"switch\", \"title_id\": \"%016llX\", \"app_id\": \"%016llX\", "
                      "\"type\": \"%s\", \"version_code\": %u, \"name\": %s, \"size\": %zu, \"format\": \"%s\", "
                      "\"sha256\": \"%s\", \"file\": %s}",
                      items.empty() ? "" : ",", (unsigned long long)id, version, (unsigned long long)title, (unsigned long long)id,
                      type, version, json_text(name).c_str(), size, file.ends_with(".nsz") ? "nsz" : "nsp", sha.hex().c_str(),
                      json_text("/" + file).c_str());
        items += line;
    };
    int served = 0;
    for (size_t k = 0; k < games.size(); k++) {
        const Game& g = games[k];
        if (!g.on_server) continue;
        const uint64_t base = id_of_game(k);
        const size_t   body = (24u << 10) + k * (4u << 10);
        const char*    ext = g.compressed ? "nsz" : "nsp";
        char           file[64];
        std::snprintf(file, sizeof file, "files/%016llX.%s", (unsigned long long)base, ext);
        const Bytes b = package(MetaType::Application, base, 0, body, g.name, g.compressed);
        write(out + "/" + file, std::string(b.begin(), b.end()));
        item(file, b.size(), base, base, 0, "base", g.name);
        for (int u = g.updates; u >= 1; u--) {
            const uint32_t version = (uint32_t)u << 16;
            std::snprintf(file, sizeof file, "files/%016llX_v%u.nsp", (unsigned long long)(base | 0x800), version);
            const Bytes p = package(MetaType::Patch, base | 0x800, version, body / 4, g.name + " update", false);
            write(out + "/" + file, std::string(p.begin(), p.end()));
            item(file, p.size(), base | 0x800, base, version, "update", g.name + " update");
        }
        for (int d = 1; d <= g.dlcs; d++) {
            const uint64_t id = base + 0x1000 + (uint64_t)d;
            std::snprintf(file, sizeof file, "files/%016llX.nsp", (unsigned long long)id);
            const Bytes p = package(MetaType::AddOnContent, id, 0, 12u << 10, g.name + " add-on", false);
            write(out + "/" + file, std::string(p.begin(), p.end()));
            const char* what[] = { " – Soundtrack", " – Extra stories", " – Season pass" };
            item(file, p.size(), id, base, 0, "dlc", g.name + what[(d - 1) % 3]);
        }
        served++;
    }
    mkdir((out + "/lantern").c_str(), 0755);
    write(out + "/lantern/handshake.json",
          "{\"schema\": 1, \"uid\": \"d0e0d0e0-0000-4000-8000-00000000d0e0\", \"name\": \"Demo shelf\", "
          "\"motd\": \"Welcome to PlayShelf's demo shelf: made-up games, installed on a simulated console.\", "
          "\"base\": \"/api/lantern\", \"versions\": [1], \"platforms\": [\"switch\"], "
          "\"account\": {\"state\": \"active\", \"scopes\": [\"library\"]}, "
          "\"features\": {\"library\": 1, \"titles\": 0, \"changes\": 0, \"sections\": 0, \"homebrew\": 1, \"deltas\": 1, \"saves\": 0, "
          "\"cheats\": 0, \"requests\": 0, \"pairing\": 0}}\n");
    write(out + "/lantern/apps.json", "{\"schema\": 1, \"items\": [" + items + "\n  ], \"next\": null}\n");

    // One homebrew, for the Homebrew tab: a made-up NRO (its header is all a
    // build needs to see) on a one-entry manifest. The server holds its
    // version 1.3.0 with a chunk index (SPEC.md 22.1); the console's SD card
    // starts with 1.2.0 (console/old/, console/data/homebrew.json), so
    // updating it reads the chunks the old copy holds and asks for the rest.
    {
        for (const char* d : { "/console", "/console/old", "/console/data" }) mkdir((out + d).c_str(), 0755);
        const std::string id = "org.example.lamplight", path = "/switch/" + id + "/" + id + ".nro";
        // 320 KiB of made-up bytes; the newer one rewrites 40 KiB in the
        // middle and adds 8 KiB at the end.
        std::string old_nro = "NRO0";
        uint32_t    x       = 0x2545f491u;
        while (old_nro.size() < (320u << 10)) {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            old_nro.push_back((char)(x >> 11));
        }
        std::string nro = old_nro;
        for (size_t k = 100u << 10; k < (140u << 10); k++) nro[k] = (char)(nro[k] ^ 0x5a);
        for (size_t k = 0; k < (8u << 10); k++) nro.push_back((char)(k * 7 + 3));
        write(out + "/console/old/" + id + ".nro", old_nro);
        write(out + "/files/" + id + ".nro", nro);
        const auto hex_of = [](const std::string& b) {
            Sha256 h;
            h.update(std::string_view(b));
            return h.hex();
        };
        const std::string fh = hex_of(nro);
        Sha256 manifest_sha;
        const std::string canonical = "[{\"path\":\"" + path + "\",\"sha256\":\"" + fh + "\",\"size\":" + std::to_string(nro.size()) + "}]";
        manifest_sha.update(std::string_view(canonical));
        // The chunk index, cut by the SDK's own chunker.
        const auto cuts = lantern::cdc_cut(std::span<const std::byte>(reinterpret_cast<const std::byte*>(nro.data()), nro.size()), 65536);
        if (!cuts) {
            std::fprintf(stderr, "web_demo: the homebrew could not be cut into chunks\n");
            return 1;
        }
        std::string index = "{\"chunker\": {\"alg\": \"lantern-cdc-1\", \"size\": 65536}, \"chunks\": [";
        for (size_t k = 0; k < cuts->size(); k++) {
            const auto& c = (*cuts)[k];
            index += std::string(k ? ", " : "") + "[" + std::to_string(c.length) + ", \"" + hex_of(nro.substr(c.offset, c.length)) + "\"]";
        }
        index += "], \"schema\": 1, \"sha256\": \"" + fh + "\", \"size\": " + std::to_string(nro.size()) + "}";
        write(out + "/files/" + id + ".chunks.json", index);
        write(out + "/lantern/homebrew.json",
              "{\"schema\": 1, \"items\": [\n    {\"id\": \"" + id + "\", \"platform\": \"switch\", \"kind\": \"app\", "
              "\"name\": \"Lamplight\", \"author\": \"Noor\", \"version\": \"1.3.0\", \"build\": 10300, "
              "\"license\": \"MIT\", \"language\": \"en\", "
              "\"icon\": {\"url\": \"/icons/" + id + ".jpg\", \"width\": 256}, "
              "\"summary\": \"A pocket journal for long nights.\", "
              "\"description\": \"Write a few lines each evening; Lamplight keeps them on the SD card.\", "
              "\"updated\": \"2026-11-05T08:00:00Z\", "
              "\"package\": {\"format\": \"nro\", \"file\": \"/files/" + id + ".nro\", \"path\": \"" + path + "\", "
              "\"size\": " + std::to_string(nro.size()) + ", \"sha256\": \"" + fh + "\", \"manifest_sha256\": \"" + manifest_sha.hex() + "\", "
              "\"chunks\": {\"url\": \"/files/" + id + ".chunks.json\", \"size\": " + std::to_string(index.size()) +
              ", \"sha256\": \"" + hex_of(index) + "\"}}}\n"
              "  ], \"next\": null}\n");
        // What the console says it installed: 1.2.0, from this server.
        write(out + "/console/data/homebrew.json",
              "{\"schema\": 1, \"installed\": [{\"id\": \"" + id + "\", \"version\": \"1.2.0\", \"platform\": \"switch\", "
              "\"server\": \"d0e0d0e0-0000-4000-8000-00000000d0e0\", \"build\": 10200, \"files\": [\"" + path + "\"]}]}\n");
    }

    // Atlas' files of every game, and the console's first ones: their
    // content meta records and names (web_demo_art.py adds their icons), and
    // the wishlist.
    mkdir((out + "/insights").c_str(), 0755);
    mkdir((out + "/insights/platforms").c_str(), 0755);
    write_insights(out + "/insights/platforms/switch", games, day);
    mkdir((out + "/console").c_str(), 0755);
    mkdir((out + "/console/meta").c_str(), 0755);
    mkdir((out + "/console/control").c_str(), 0755);
    mkdir((out + "/console/data").c_str(), 0755);
    std::string wished;
    for (size_t k = 0; k < games.size(); k++) {
        if (games[k].wished) wished += hex_id(id_of_game(k)) + "\n";
        if (!games[k].installed) continue;
        char meta[64], control[64];
        std::snprintf(meta, sizeof meta, "/console/meta/%016llx-0.bin", (unsigned long long)id_of_game(k));
        std::snprintf(control, sizeof control, "/console/control/%016llx.txt", (unsigned long long)id_of_game(k));
        write(out + meta, "");
        write(out + control, games[k].name + "\n");
    }
    write(out + "/console/data/wishlist.txt", text_schema::with(library::IDS_SCHEMA, wished));
    std::printf("web_demo: %d games on the server, %zu in all, %u contents, in %s\n", served, games.size(),
                (unsigned)(s_next_id - 0x10), out.c_str());
    return 0;
}
