// The web simulator's library server (web/README.md): a catalogue of made-up
// games, their packages and icons, written to <out-dir> and packaged with
// the simulator at /app/samples/demo, which source/sim/web_http.cpp serves
// as https://demo.playshelf.example. The packages are built as the install
// suites build theirs (tests/install/packages.hpp: the test keys, which the
// simulated console takes), small: what they install is a stand-in. Built
// and run on the host by `make web` (it needs libzstd and zlib).
//
//   web_demo <out-dir>
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cmath>
#include <cstdio>
#include <fstream>
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
};

// Made up, as their icons are.
const Game GAMES[] = {
    { "Starlight Garden", "2025-03-14", 0.55f, 0.75f, 2, 2, false },
    { "Copper Canyon Rally", "2024-11-02", 0.05f, 0.12f, 1, 0, false },
    { "Lanterns of Avel", "2023-06-21", 0.62f, 0.85f, 0, 1, true },
    { "Tiny Harbour", "2026-01-30", 0.48f, 0.58f, 1, 0, false },
    { "Moss & Mortar", "2022-09-09", 0.28f, 0.36f, 0, 2, false },
    { "Pixel Parade", "2025-07-18", 0.88f, 0.98f, 0, 0, false },
    { "Orbit Kitchen", "2024-04-04", 0.10f, 0.95f, 1, 0, false },
    { "The Quiet Lighthouse", "2021-12-12", 0.58f, 0.66f, 0, 0, true },
    { "Fennec Fables", "2025-10-10", 0.08f, 0.15f, 0, 1, false },
    { "Glacier Run", "2023-02-17", 0.52f, 0.60f, 2, 0, false },
    { "Paper Kingdoms", "2026-05-05", 0.00f, 0.08f, 0, 0, false },
    { "Nightbloom Tactics", "2024-08-28", 0.75f, 0.92f, 1, 1, false },
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
    write(out + "/index.json", "{\n  \"success\": \"Welcome to PlayShelf's demo shelf: made-up games, installed on a simulated console.\",\n"
                               "  \"files\": [" + items + "\n  ]\n}\n");
    std::printf("web_demo: %d games, %u contents, in %s\n", k, (unsigned)(s_next_id - 0x10), out.c_str());
    return 0;
}
