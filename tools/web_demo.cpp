// The web simulator's library server (web/README.md): a catalogue of made-up
// games, their packages and icons, written to <out-dir> and packaged with
// the simulator at /app/samples/demo, which source/sim/web_http.cpp serves
// as https://demo.playshelf.example. The packages are built as the install
// suites build theirs (tests/install/packages.hpp: the test keys, which the
// simulated console takes), small: what they install is a stand-in. Built
// and run on the host by `make web` (it needs libzstd and zlib).
//
// With them, what Home reads of PlayInsights about those games, made up as
// well (insights/: popular.json, games.json and similar.json, in
// PlayInsights' schema 1), which web_http.cpp answers in place of
// PlayInsights' own, and the console's first games (console/meta/: a content
// meta record for each, empty; console/control/: its name and icon), which
// the page puts on the simulated console at a first start with the demo
// (source/sim/web_seed.cpp): so Home shows its rows, the recommendations
// and their reasons among them.
//
//   web_demo <out-dir>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <zlib.h>

#include "install/packages.hpp"

using namespace install;
using namespace test_install;

namespace
{
struct Game
{
    const char* name;
    const char* date;          // its release, YYYY-MM-DD
    float       hue_a, hue_b;  // its icon's colours (0..1)
    int         updates;       // newest first: 2 → v131072 and v65536
    int         dlcs;
    bool        compressed;    // an .nsz
    // What PlayInsights would say of it: its series ("" for none), its
    // studio, its genres (", " between them), how well known it is, how
    // fast that grows and the critics' score (popular.json), its Steam
    // reviews (0 and 0: not on Steam).
    const char* series;
    const char* studio;
    const char* genres;
    int         score, trend, critics;
    int         positive, negative;
    bool        installed;     // on the console at a first start
};

// Made up, as their icons are: games like today's best known ones (an open
// world, a kart race, monsters to collect, a village to live in…), of a few
// made-up series and studios.
const Game GAMES[] = {
    { "Starlight Garden", "2025-03-14", 0.55f, 0.75f, 2, 2, false, "", "Fennec Games", "life simulation", 34, 4, 78, 0, 0, false },
    { "Copper Canyon Rally", "2024-11-02", 0.05f, 0.12f, 1, 0, false, "", "Copperline", "racing", 22, 0, 71, 8400, 1300, false },
    { "Lanterns of Avel", "2023-06-21", 0.62f, 0.85f, 0, 1, true, "Lanterns of Avel", "Lumen Works", "metroidvania, platformer", 48, 0, 88, 31000, 1500, true },
    { "Tiny Harbour", "2026-01-30", 0.48f, 0.58f, 1, 0, false, "Tiny Harbour", "Northfox", "life simulation", 66, 35, 84, 0, 0, false },
    { "Moss & Mortar", "2022-09-09", 0.28f, 0.36f, 0, 2, false, "", "Mapleleaf", "farming, life simulation", 52, 0, 89, 96000, 3100, true },
    { "Pixel Parade", "2025-07-18", 0.88f, 0.98f, 0, 0, false, "", "Fennec Games", "party, platformer", 40, 12, 74, 0, 0, false },
    { "Orbit Kitchen", "2024-04-04", 0.10f, 0.95f, 1, 0, false, "", "Fennec Games", "party, cooking", 37, 0, 80, 12000, 900, true },
    { "The Quiet Lighthouse", "2021-12-12", 0.58f, 0.66f, 0, 0, true, "", "Lumen Works", "puzzle, adventure", 18, 0, 82, 5100, 200, false },
    { "Fennec Fables", "2025-10-10", 0.08f, 0.15f, 0, 1, false, "", "Fennec Games", "platformer", 30, 25, 79, 0, 0, false },
    { "Glacier Run", "2023-02-17", 0.52f, 0.60f, 2, 0, false, "", "Copperline", "racing, sports", 20, 0, 68, 2300, 700, false },
    { "Paper Kingdoms", "2026-05-05", 0.00f, 0.08f, 0, 0, false, "", "Inkwell", "strategy", 28, 40, 77, 4100, 600, false },
    { "Nightbloom Tactics", "2024-08-28", 0.75f, 0.92f, 1, 1, false, "", "Ashen Pixel", "tactical role-playing, strategy", 35, 0, 85, 15000, 1100, false },
    { "Sky Kingdom: Ballad of Wings", "2023-05-12", 0.30f, 0.50f, 1, 1, false, "Sky Kingdom", "Northfox", "action-adventure, open world", 95, 8, 96, 0, 0, true },
    { "Sky Kingdom: Winds of Dawn", "2017-03-03", 0.25f, 0.42f, 2, 2, false, "Sky Kingdom", "Northfox", "action-adventure, open world", 92, 0, 97, 0, 0, false },
    { "Turbo Kart Grand Prix", "2017-04-28", 0.95f, 0.05f, 2, 1, false, "Turbo Kart", "Northfox", "kart racing, racing, party", 90, 0, 92, 0, 0, true },
    { "Turbo Kart World", "2025-06-05", 0.98f, 0.10f, 1, 0, false, "Turbo Kart", "Northfox", "kart racing, racing, open world", 86, 30, 86, 0, 0, false },
    { "Pocket Critters: Ember", "2022-11-18", 0.02f, 0.10f, 1, 2, false, "Pocket Critters", "Pebble Forge", "monster collecting, role-playing", 88, 0, 72, 0, 0, true },
    { "Pocket Critters: Frost", "2026-10-16", 0.55f, 0.62f, 0, 0, false, "Pocket Critters", "Pebble Forge", "monster collecting, role-playing", 80, 60, 81, 0, 0, false },
    { "Ink Squad", "2022-09-09", 0.80f, 0.20f, 2, 1, false, "", "Inkwell", "shooter, party", 72, 0, 83, 0, 0, false },
    { "Ultimate Brawl Legends", "2018-12-07", 0.00f, 0.60f, 2, 3, false, "", "Northfox", "fighting, party", 89, 0, 93, 0, 0, false },
    { "Starhop Voyage", "2017-10-27", 0.12f, 0.70f, 0, 1, false, "Starhop", "Northfox", "platformer, 3D platformer", 84, 0, 97, 0, 0, false },
    { "The Ashen Descent", "2020-09-17", 0.03f, 0.85f, 1, 0, false, "", "Ashen Pixel", "roguelike, action", 70, 0, 93, 210000, 4200, false },
    { "Sunpetal Farm", "2024-03-20", 0.15f, 0.30f, 1, 1, false, "", "Mapleleaf", "farming, life simulation", 45, 18, 81, 22000, 1400, false },
    { "Summit Ascent", "2018-01-25", 0.70f, 0.80f, 0, 1, false, "", "Lumen Works", "platformer, precision platformer", 55, 0, 92, 74000, 2000, false },
};

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

// ------------------------------------------------------------- the icons

void rgb_of(float h, float s, float v, uint8_t* out)
{
    h = h - std::floor(h);
    const int   i = (int)(h * 6);
    const float f = h * 6 - (float)i, p = v * (1 - s), q = v * (1 - f * s), u = v * (1 - (1 - f) * s);
    float       r, g, b;
    switch (i % 6) {
    case 0: r = v, g = u, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = u; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = u, g = p, b = v; break;
    default: r = v, g = p, b = q; break;
    }
    out[0] = (uint8_t)(r * 255), out[1] = (uint8_t)(g * 255), out[2] = (uint8_t)(b * 255);
}

void be32(std::string* b, uint32_t v)
{
    for (int i = 3; i >= 0; i--) b->push_back((char)(v >> (8 * i)));
}

void chunk(std::string* png, const char* type, const std::string& data)
{
    be32(png, (uint32_t)data.size());
    const std::string body = std::string(type, 4) + data;
    png->append(body);
    be32(png, (uint32_t)crc32(0, (const Bytef*)body.data(), (uInt)body.size()));
}

// A 256×256 icon: a diagonal of its two colours, a ring and a dot in it.
std::string icon(const Game& g, int index)
{
    const int   n = 256;
    std::string raw;
    for (int y = 0; y < n; y++) {
        raw.push_back(0);   // no filter
        for (int x = 0; x < n; x++) {
            const float t = (float)(x + y) / (2.0f * n);
            uint8_t     px[3];
            rgb_of(g.hue_a + (g.hue_b - g.hue_a) * t, 0.55f, 0.95f - 0.35f * t, px);
            const float cx = 128 + 30 * std::cos((float)index), cy = 128 + 30 * std::sin((float)index);
            const float d = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            if ((d > 70 && d < 84) || d < 26)
                for (uint8_t& c : px) c = (uint8_t)(c + (255 - c) * 0.85f);
            raw.append((const char*)px, 3);
        }
    }
    uLongf      size = compressBound((uLong)raw.size());
    std::string z(size, '\0');
    compress2((Bytef*)z.data(), &size, (const Bytef*)raw.data(), (uLong)raw.size(), 9);
    z.resize(size);
    std::string png("\x89PNG\r\n\x1a\n", 8), ihdr;
    be32(&ihdr, n);
    be32(&ihdr, n);
    ihdr += std::string("\x08\x02\x00\x00\x00", 5);   // 8 bits, RGB
    chunk(&png, "IHDR", ihdr);
    chunk(&png, "IDAT", z);
    chunk(&png, "IEND", "");
    return png;
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
// ------------------------------------------------------ PlayInsights' files

// A game's traits of each kind: [id, label] pairs. The ids are made up too
// (Q and a number from 9 000 000), one for each label of a kind.
using Traits = std::map<std::string, std::vector<std::string>>;   // kind: labels

std::vector<std::string> list_of(const char* text)
{
    std::vector<std::string> out;
    std::string              s = text;
    size_t                   at = 0;
    while (at < s.size()) {
        size_t end = s.find(", ", at);
        if (end == std::string::npos) end = s.size();
        if (end > at) out.push_back(s.substr(at, end - at));
        at = end + 2;
    }
    return out;
}

Traits traits_of(const Game& g)
{
    Traits t;
    if (*g.series) t["series"] = { g.series };
    t["developers"] = { g.studio };
    t["publishers"] = { g.studio };
    t["genres"] = list_of(g.genres);
    t["modes"] = { "single-player video game" };
    if (std::strstr(g.genres, "party") || std::strstr(g.genres, "racing") || std::strstr(g.genres, "fighting"))
        t["modes"].push_back("multiplayer video game");
    return t;
}

std::string trait_id(const std::string& kind, const std::string& label)
{
    static std::map<std::string, int> ids;
    const auto it = ids.emplace(kind + "\n" + label, 9000000 + (int)ids.size()).first;
    return "Q" + std::to_string(it->second);
}

// How much sharing a trait of a kind counts, as PlayInsights weighs it
// (its tools/features.py): a series most, a mode least.
double kind_weight(const std::string& kind)
{
    return kind == "series" ? 4.0 : kind == "developers" ? 2.0 : kind == "genres" ? 1.0 : 0.5;
}

// Each game's most similar ones, 1..100, as PlayInsights finds them: a
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

// popular.json, games.json and similar.json of the games `ids` names, made
// on `day` (today: newer than the files PlayShelf ships, which it takes in
// their place).
void write_insights(const std::string& dir, const std::vector<uint64_t>& ids, const std::string& day)
{
    mkdir(dir.c_str(), 0755);
    const std::string head = "{\"schema\": 1, \"generated\": \"" + day + "\", \"source\": \"made up for the demo\", ";
    std::string popular = head + "\"month\": \"" + day.substr(0, 7) + "\", \"views\": true, \"games\": {";
    std::string games = head + "\"games\": {";
    std::string similar = head + "\"games\": {";
    std::vector<Traits> all;
    for (const Game& g : GAMES) all.push_back(traits_of(g));
    const auto like = similar_of(all);
    for (size_t i = 0; i < ids.size(); i++) {
        const Game&       g = GAMES[i];
        const std::string sep = i ? ", " : "", key = json_text(hex_id(ids[i]));
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
        games += "}";
        similar += sep + key + ": [";
        for (size_t k = 0; k < like[i].size(); k++)
            similar += std::string(k ? ", " : "") + "[" + json_text(hex_id(ids[like[i][k].first])) + ", " + std::to_string(like[i][k].second) + "]";
        similar += "]";
    }
    write(dir + "/popular.json", popular + "}}\n");
    write(dir + "/games.json", games + "}}\n");
    write(dir + "/similar.json", similar + "}}\n");
}
}   // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: web_demo <out-dir>\n");
        return 2;
    }
    const std::string out = argv[1];
    mkdir(out.c_str(), 0755);
    mkdir((out + "/files").c_str(), 0755);
    mkdir((out + "/icons").c_str(), 0755);
    std::string items;
    auto        item = [&items](const std::string& url, size_t size, uint64_t id, uint32_t version, const char* type,
                         const std::string& name, const std::string& icon_url, const char* date) {
        char line[1024];
        std::snprintf(line, sizeof line,
                      "%s\n    {\"url\": %s, \"size\": %zu, \"title_id\": \"%016llX\", \"app_version\": %u, \"app_type\": \"%s\"%s%s%s%s%s}",
                      items.empty() ? "" : ",", json_text(url).c_str(), size, (unsigned long long)id, version, type,
                      name.empty() ? "" : ", \"name\": ", name.empty() ? "" : json_text(name).c_str(),
                      icon_url.empty() ? "" : ", \"icon_url\": ", icon_url.empty() ? "" : json_text(icon_url).c_str(),
                      date ? (std::string(", \"release_date\": \"") + date + "\"").c_str() : "");
        items += line;
    };
    int k = 0;
    for (const Game& g : GAMES) {
        const uint64_t base = 0x0100DE0000000000ull | ((uint64_t)(k + 1) << 16);
        const size_t   body = (24u << 10) + (size_t)k * (4u << 10);
        const char*    ext = g.compressed ? "nsz" : "nsp";
        char           file[64];
        std::snprintf(file, sizeof file, "files/%016llX.%s", (unsigned long long)base, ext);
        const Bytes b = package(MetaType::Application, base, 0, body, g.name, g.compressed);
        write(out + "/" + file, std::string(b.begin(), b.end()));
        char icon_file[64];
        std::snprintf(icon_file, sizeof icon_file, "icons/%016llX.png", (unsigned long long)base);
        write(out + "/" + icon_file, icon(g, k));
        item(file, b.size(), base, 0, "BASE", g.name, icon_file, g.date);
        for (int u = g.updates; u >= 1; u--) {
            const uint32_t version = (uint32_t)u << 16;
            std::snprintf(file, sizeof file, "files/%016llX_v%u.nsp", (unsigned long long)(base | 0x800), version);
            const Bytes p = package(MetaType::Patch, base | 0x800, version, body / 4, std::string(g.name) + " update", false);
            write(out + "/" + file, std::string(p.begin(), p.end()));
            item(file, p.size(), base | 0x800, version, "UPDATE", "", "", nullptr);
        }
        for (int d = 1; d <= g.dlcs; d++) {
            const uint64_t id = base + 0x1000 + (uint64_t)d;
            std::snprintf(file, sizeof file, "files/%016llX.nsp", (unsigned long long)id);
            const Bytes p = package(MetaType::AddOnContent, id, 0, 12u << 10, std::string(g.name) + " add-on", false);
            write(out + "/" + file, std::string(p.begin(), p.end()));
            item(file, p.size(), id, 0, "DLC", std::string(g.name) + (d == 1 ? " – Soundtrack" : " – Extra stories"), "", nullptr);
        }
        k++;
    }
    // PlayInsights' files of these games, and the console's first ones.
    std::vector<uint64_t> ids;
    for (int i = 0; i < k; i++) ids.push_back(0x0100DE0000000000ull | ((uint64_t)(i + 1) << 16));
    const std::time_t now = std::time(nullptr);
    char              day[16];
    std::strftime(day, sizeof day, "%Y-%m-%d", std::gmtime(&now));
    write_insights(out + "/insights", ids, day);
    mkdir((out + "/console").c_str(), 0755);
    mkdir((out + "/console/meta").c_str(), 0755);
    mkdir((out + "/console/control").c_str(), 0755);
    for (int i = 0; i < k; i++)
        if (GAMES[i].installed) {
            char meta[64], control[64];
            std::snprintf(meta, sizeof meta, "/console/meta/%016llx-0.bin", (unsigned long long)ids[(size_t)i]);
            std::snprintf(control, sizeof control, "/console/control/%016llx", (unsigned long long)ids[(size_t)i]);
            write(out + meta, "");
            write(out + control + ".txt", std::string(GAMES[i].name) + "\n");
            write(out + control + ".png", icon(GAMES[i], i));
        }
    write(out + "/index.json", "{\n  \"success\": \"Welcome to PlayShelf's demo shelf: made-up games, installed on a simulated console.\",\n"
                               "  \"files\": [" + items + "\n  ]\n}\n");
    std::printf("web_demo: %d games, %u contents, in %s\n", k, (unsigned)(s_next_id - 0x10), out.c_str());
    return 0;
}
