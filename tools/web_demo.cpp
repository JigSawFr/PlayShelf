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
// well (insights/switch/: popular.json, games.json, similar.json and
// editions.json, in Atlas' schema 1 and its switch/ folder), which sim_demo_http.cpp answers in place of
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
#include <ctime>
#include <fstream>
#include <map>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <aurore/extern/nlohmann/json.hpp>

#include "install/packages.hpp"
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

// popular.json, games.json, similar.json and editions.json of the table's
// games, made on `day` (today: newer than the files PlayShelf ships, which
// it takes in their place).
void write_insights(const std::string& dir, const std::vector<Game>& table, const std::string& day)
{
    mkdir(dir.c_str(), 0755);
    const std::string head = "{\"schema\": 1, \"generated\": \"" + day + "\", \"source\": \"made up for the demo\", ";
    std::string popular = head + "\"month\": \"" + day.substr(0, 7) + "\", \"views\": true, \"games\": {";
    std::string games = head + "\"games\": {";
    std::string similar = head + "\"games\": {";
    std::string editions = head + "\"games\": {";
    std::vector<Traits> all;
    for (const Game& g : table) all.push_back(traits_of(g));
    const auto like = similar_of(all);
    for (size_t i = 0; i < table.size(); i++) {
        const Game&       g = table[i];
        const std::string sep = i ? ", " : "", key = json_text(hex_id(id_of_game(i)));
        popular += sep + key + ": [" + std::to_string(g.score) + ", " + std::to_string(g.trend) + ", " + std::to_string(g.critics) + "]";
        games += sep + key + ": {";
        bool first = true;
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
        games += "}";
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
    const std::time_t now = std::time(nullptr);
    char              day[16];
    std::strftime(day, sizeof day, "%Y-%m-%d", std::gmtime(&now));
    const std::vector<Game> games = read_games(argv[2], day);
    mkdir(out.c_str(), 0755);
    mkdir((out + "/files").c_str(), 0755);
    std::string items;
    auto        item = [&items](const std::string& url, size_t size, uint64_t id, uint32_t version, const char* type,
                         const std::string& name, const std::string& icon_url, const std::string& date) {
        char line[1024];
        std::snprintf(line, sizeof line,
                      "%s\n    {\"url\": %s, \"size\": %zu, \"title_id\": \"%016llX\", \"app_version\": %u, \"app_type\": \"%s\"%s%s%s%s%s}",
                      items.empty() ? "" : ",", json_text(url).c_str(), size, (unsigned long long)id, version, type,
                      name.empty() ? "" : ", \"name\": ", name.empty() ? "" : json_text(name).c_str(),
                      icon_url.empty() ? "" : ", \"icon_url\": ", icon_url.empty() ? "" : json_text(icon_url).c_str(),
                      date.empty() ? "" : (", \"release_date\": \"" + date + "\"").c_str());
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
        // Its icon: tools/web_demo_art.py's.
        char icon_file[64];
        std::snprintf(icon_file, sizeof icon_file, "icons/%016llX.jpg", (unsigned long long)base);
        item(file, b.size(), base, 0, "BASE", g.name, icon_file, g.date);
        for (int u = g.updates; u >= 1; u--) {
            const uint32_t version = (uint32_t)u << 16;
            std::snprintf(file, sizeof file, "files/%016llX_v%u.nsp", (unsigned long long)(base | 0x800), version);
            const Bytes p = package(MetaType::Patch, base | 0x800, version, body / 4, g.name + " update", false);
            write(out + "/" + file, std::string(p.begin(), p.end()));
            item(file, p.size(), base | 0x800, version, "UPDATE", "", "", "");
        }
        for (int d = 1; d <= g.dlcs; d++) {
            const uint64_t id = base + 0x1000 + (uint64_t)d;
            std::snprintf(file, sizeof file, "files/%016llX.nsp", (unsigned long long)id);
            const Bytes p = package(MetaType::AddOnContent, id, 0, 12u << 10, g.name + " add-on", false);
            write(out + "/" + file, std::string(p.begin(), p.end()));
            const char* what[] = { " – Soundtrack", " – Extra stories", " – Season pass" };
            item(file, p.size(), id, 0, "DLC", g.name + what[(d - 1) % 3], "", "");
        }
        served++;
    }
    write(out + "/index.json", "{\n  \"success\": \"Welcome to PlayShelf's demo shelf: made-up games, installed on a simulated console.\",\n"
                               "  \"files\": [" + items + "\n  ]\n}\n");

    // Atlas' files of every game, and the console's first ones: their
    // content meta records and names (web_demo_art.py adds their icons), and
    // the wishlist.
    mkdir((out + "/insights").c_str(), 0755);
    write_insights(out + "/insights/switch", games, day);
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
