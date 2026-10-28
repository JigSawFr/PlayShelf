// titledb/: CyberFoil-DB's packs read (a title's name, publisher, version;
// an icon's bytes), the ones whose index does not hold together refused, a
// pack's text made safe; its manifest read with its aliases, relative
// addresses resolved, the ones not HTTPS refused; a title list (a JSON
// object of title ids) read as it streams and made a titles pack; Nlib API's icon
// addresses.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "temp_dir.hpp"
#include "titledb/list.hpp"
#include "titledb/manifest.hpp"
#include "titledb/nlib.hpp"
#include "titledb/pack.hpp"
#include "titledb/packs.hpp"

#include <cstring>
#include <fstream>
#include <map>

using namespace titledb;
using namespace packs;

namespace
{
void test_titles()
{
    TempDir tmp("titledb", TempDir::Enter);
    // Out of order, as a custom pack may be.
    write("t.pack", titles_pack({ { 0x0100000000020000, "Zeta\nGame", "Studio", 0 },
                                  { 0x0100000000010000, "Alpha", "", 0 },
                                  { 0x0100000000010800, "", "", 393216 } }));
    TitlePack   p;
    std::string why;
    if (!CHECK(p.open("t.pack", &why))) return;
    CHECK(p.count() == 3);
    const auto a = p.find(0x0100000000010000);
    CHECK(a && a->name == "Alpha" && a->publisher.empty() && a->size == 1234);
    // A control character made a space.
    const auto z = p.find(0x0100000000020000);
    CHECK(z && z->name == "Zeta Game" && z->publisher == "Studio");
    // An update: its version only.
    const auto u = p.find(0x0100000000010800);
    CHECK(u && u->name.empty() && u->version && *u->version == 393216);
    CHECK(!p.find(0x0100000000030000));

    // Not a pack, too short, an index past its strings.
    write("bad.pack", "CFTITLE2" + std::string(40, '\0'));
    TitlePack q;
    CHECK(!q.open("bad.pack", &why) && !why.empty());
    write("short.pack", "CFTI");
    CHECK(!q.open("short.pack", &why));
    std::string broken = titles_pack({ { 1, "x", "", 0 } });
    broken[16] = 100;   // a hundred entries said, one there
    write("broken.pack", broken);
    CHECK(!q.open("broken.pack", &why));
    CHECK(!q.open("missing.pack", &why));
    // Strings said past the file's end.
    write("far.pack", titles_pack({ { 1, "x", "", 0 } }, 1000));
    CHECK(!q.open("far.pack", &why));
}

void test_icons()
{
    TempDir tmp("titledb_icons", TempDir::Enter);
    write("i.pack", icons_pack({ { 0x0100000000010000, "RIFFicon1" }, { 0x0100000000020000, "RIFFicon2!" } }));
    IconPack    p;
    std::string why;
    if (!CHECK(p.open("i.pack", &why))) return;
    CHECK(p.count() == 2 && p.has(0x0100000000020000) && !p.has(3));
    std::vector<uint8_t> b;
    std::string          ext;
    CHECK(p.find(0x0100000000020000, &b, &ext) && std::string(b.begin(), b.end()) == "RIFFicon2!" && ext == "webp");
    CHECK(!p.find(5, &b, &ext));
    // An entry past the file's end: left out.
    std::string cut = icons_pack({ { 1, "abc" }, { 2, "defg" } });
    cut.resize(cut.size() - 2);
    write("cut.pack", cut);
    IconPack c;
    CHECK(c.open("cut.pack", &why) && c.has(1) && !c.has(2));
}

void test_safe_text()
{
    CHECK(safe_text("a\tb\x7f") == "a b ");
    CHECK(safe_text("Pokémon") == "Pokémon");
    CHECK(safe_text("\xff\xfe").empty());
    CHECK(safe_text("\xc3").empty());
    // Overlong, a surrogate, past U+10FFFF.
    CHECK(safe_text("\xc0\xaf").empty() && safe_text("\xed\xa0\x80").empty() && safe_text("\xf4\x90\x80\x80").empty());
    // Cut on a character.
    CHECK(safe_text("ééé", 5) == "éé");
}

void test_manifest()
{
    const std::string base = "https://github.com/o/r/releases/latest/download/offline_db_manifest.json";
    const std::string sha(64, 'A');
    Manifest          m;
    std::string       why;
    CHECK(parse_manifest(R"({"schema":1,"db_version":"20261013","files":{"titles.pack":{"url":"https://x.test/t.pack","size":10,"sha256":")" +
                             sha + R"("},"icons.pack":{"url":"icons.pack","size":20,"sha256":")" + sha + R"("}}})",
                         base, &m, &why));
    CHECK(m.version == "20261013" && m.titles.url == "https://x.test/t.pack" && m.titles.size == 10);
    CHECK(m.titles.sha256 == std::string(64, 'a'));
    CHECK(m.icons.url == "https://github.com/o/r/releases/latest/download/icons.pack" && m.icons.size == 20);
    // The aliases, and no icons.
    CHECK(parse_manifest(R"({"version":7,"titles_pack":{"path":"/db/t.pack","size":5,"sha_256":")" + sha + R"("}})",
                         "https://h.test:8443/a/m.json", &m, &why));
    CHECK(m.version == "7" && m.titles.url == "https://h.test:8443/db/t.pack" && !m.icons.known());
    // Refused: HTTP, no hash, no size, no titles, not JSON.
    CHECK(!parse_manifest(R"({"version":"1","files":{"titles.pack":{"url":"http://x/t","size":5,"sha256":")" + sha + R"("}}})", base, &m, &why) &&
          why.find("HTTPS") != std::string::npos);
    CHECK(!parse_manifest(R"({"version":"1","files":{"titles.pack":{"url":"t","size":5}}})", base, &m, &why));
    CHECK(!parse_manifest(R"({"version":"1","files":{"titles.pack":{"url":"t","sha256":")" + sha + R"("}}})", base, &m, &why));
    CHECK(!parse_manifest(R"({"version":"1","files":{}})", base, &m, &why));
    CHECK(!parse_manifest("<html>", base, &m, &why));
    // A relative address against an HTTP manifest: not HTTPS either.
    CHECK(!parse_manifest(R"({"version":"1","titles":{"url":"t","size":5,"sha256":")" + sha + R"("}})", "http://h/m.json", &m, &why));
    CHECK(resolve("https://h/a/b.json?x=1", "c.pack") == "https://h/a/c.pack");
    CHECK(resolve("https://h", "c.pack") == "https://h/c.pack");
}
void test_list()
{
    TempDir                tmp("titledb_list", TempDir::Enter);
    std::vector<ListEntry> e;
    std::string            why;
    // Ids at the top: versions as text, a size; an id twice (the last kept), a
    // key that is no title id, a name with a control character.
    CHECK(parse_title_list(R"({"0100000000010800": {"Game Name": "Mario", "Version": "393216", "Size": 306880574},
        "0100000000010000": {"Game Name": "Super Mario Odyssey™", "Version": "0", "Size": 3751207164},
        "readme": {"Game Name": "not a game"},
        "01000000160F6000": {"Game Name": "Old"},
        "01000000160F6000": {"Game Name": "Bouncy\tBullets 2", "Version": 65536, "extra": [1, {"a": 2}]}})",
                           &e, &why));
    if (!CHECK(e.size() == 3)) return;
    CHECK(e[0].id == 0x0100000000010000 && e[0].name == "Super Mario Odyssey™" && e[0].has_version && e[0].version == 0 &&
          e[0].size == 3751207164ull);
    CHECK(e[1].id == 0x0100000000010800 && e[1].version == 393216);
    CHECK(e[2].id == 0x01000000160F6000 && e[2].name == "Bouncy Bullets 2" && e[2].version == 65536 && !e[2].size);

    // Made a pack, read as CyberFoil-DB's are.
    write("list.pack", titles_pack_of(e));
    TitlePack p;
    if (!CHECK(p.open("list.pack", &why))) return;
    CHECK(p.count() == 3);
    const auto t = p.find(0x0100000000010000);
    CHECK(t && t->name == "Super Mario Odyssey™" && t->size == 3751207164ull && t->version && *t->version == 0);
    CHECK(p.find(0x0100000000010800)->name == "Mario" && !p.find(0x0100000000020000));

    // A Tinfoil index's "titledb": its other keys aside.
    CHECK(parse_title_list(R"({"files": [{"url": "x"}], "titledb": {
        "0100FFC01BE60000": {"id": "0100FFC01BE60000", "name": "My Child Lebensborn Remastered", "publisher": "Sarepta Studio",
                             "releaseDate": "20231220", "description": "long\ntext", "version": 0, "size": 1234,
                             "iconUrl": "https://api.example/icon", "screenshots": ["https://a", "https://b"]},
        "0100FFC01BE60800": {"name": "My Child Lebensborn Remastered", "version": "131072", "releaseDate": 20240105}},
        "success": "hello"})",
                           &e, &why));
    if (CHECK(e.size() == 2)) {
        CHECK(e[0].name == "My Child Lebensborn Remastered" && e[0].publisher == "Sarepta Studio" && e[0].release_date == 20231220 &&
              e[0].has_version && e[0].version == 0 && e[0].size == 1234);
        CHECK(e[1].version == 131072 && e[1].release_date == 20240105 && e[1].publisher.empty());
        write("tinfoil.pack", titles_pack_of(e));
        TitlePack q;
        const auto t2 = q.open("tinfoil.pack", &why) ? q.find(0x0100FFC01BE60000) : std::nullopt;
        CHECK(t2 && t2->publisher == "Sarepta Studio" && t2->release_date == 20231220);
        // Released in a span: the base games only (not the update dated 2024).
        std::vector<uint64_t> out;
        q.released(20231201, 20241231, 10, &out);
        CHECK(out.size() == 1 && out[0] == 0x0100FFC01BE60000);
        out.clear();
        q.released(20240101, 20241231, 10, &out);
        CHECK(out.empty());
        q.released(20231201, 20231231, 0, &out);
        CHECK(out.empty());
    }

    // Not a list: an array, no title, not JSON.
    CHECK(!parse_title_list(R"([{"Game Name": "x"}])", &e, &why) && e.empty());
    CHECK(!parse_title_list(R"({"readme": {"Game Name": "x"}})", &e, &why));
    CHECK(!parse_title_list("{\"0100000000010000\": {", &e, &why));
}

void test_nlib()
{
    CHECK(nlib_icon_url(0x01007EF00011E000) == "https://api.nlib.cc/nx/01007EF00011E000/icon/256");
    CHECK(nlib_icon_url(0x0100000000010000, 30) == "https://api.nlib.cc/nx/0100000000010000/icon/30");
    // Neither an update nor an add-on: Nlib API knows base games only.
    CHECK(nlib_icon_url(0x01007EF00011E800).empty());
    CHECK(nlib_icon_url(0x01007EF00011F001).empty());
    CHECK(nlib_icon_url(0).empty());
    // A size out of the API's range.
    CHECK(nlib_icon_url(0x0100000000010000, 29).empty() && nlib_icon_url(0x0100000000010000, 4097).empty());

    // Its language: PlayShelf's, its region dropped; English for another.
    CHECK(nlib_language("fr-CA") == "fr" && nlib_language("pt-BR") == "pt" && nlib_language("zh-Hant") == "zh");
    CHECK(nlib_language("en-US") == "en" && nlib_language("ja") == "ja" && nlib_language("pl") == "en");
    CHECK(nlib_details_url(0x01007EF00011E000, "fr-CA") ==
          "https://api.nlib.cc/nx/01007EF00011E000?lang=fr&fields=name,publisher,developer,releaseDate,category,intro,"
          "description,screens,numberOfPlayers,languages,ratingContent,isDemo,nsuId");
    CHECK(nlib_details_url(0x01007EF00011E800, "fr").empty());
    CHECK(nlib_banner_url(0x01007EF00011E000) == "https://api.nlib.cc/nx/01007EF00011E000/banner/480p");
    CHECK(nlib_screen_url(0x01007EF00011E000, 1) == "https://api.nlib.cc/nx/01007EF00011E000/screen/1");
    CHECK(nlib_screen_url(0x01007EF00011E000, 0).empty() && nlib_screen_url(0x01007EF00011E000, NLIB_MAX_SCREENS + 1).empty());

    // A game's details, as the API answers them.
    Details d;
    CHECK(parse_nlib_details(R"({"id":"0100000000010000","name":"Copper\tCanyon","publisher":"Studio",
        "releaseDate":"2024-05-17","category":["Racing",4,"","Arcade"],"intro":"Dust.\r\nOne more lap? ",
        "description":"Race.\n\n\n\nTwelve tracks\nfour seasons\n\n","screens":{"count":12,"screenshots":[]},
        "banner":"http://api.nlib.cc/nx/0100000000010000/banner"})", 0x0100000000010000, &d));
    CHECK(d.name == "Copper Canyon" && d.publisher == "Studio" && d.release_date == "2024-05-17");
    CHECK(d.categories == (std::vector<std::string>{ "Racing", "Arcade" }));
    // Its line breaks kept, one blank line at most between paragraphs.
    CHECK(d.intro == "Dust.\nOne more lap?");
    CHECK(d.description == "Race.\n\nTwelve tracks\nfour seasons");
    CHECK(d.screens == NLIB_MAX_SCREENS);
    // What else it knows: its players, languages (each once), rating, eShop id.
    CHECK(parse_nlib_details(R"({"id":"0100000000010000","developer":"Dev","numberOfPlayers":4,
        "languages":["ja","en","zh","zh",""],"ratingContent":["Fantasy Violence"],"isDemo":true,"nsuId":"70010000000025"})",
                             0x0100000000010000, &d));
    CHECK(d.developer == "Dev" && d.players == 4 && d.demo && d.nsu_id == 70010000000025ull);
    CHECK(d.languages == (std::vector<std::string>{ "ja", "en", "zh" }) && d.rating == (std::vector<std::string>{ "Fantasy Violence" }));
    // Not what they should be: left unknown.
    CHECK(parse_nlib_details(R"({"id":"0100000000010000","numberOfPlayers":"two","isDemo":1,"nsuId":"70A","languages":"en"})",
                             0x0100000000010000, &d));
    CHECK(d.players == 0 && !d.demo && d.nsu_id == 0 && d.languages.empty());
    // A date that is not one, no screenshots.
    CHECK(parse_nlib_details(R"({"id":"0100000000010000","releaseDate":"soon"})", 0x0100000000010000, &d));
    CHECK(d.release_date.empty() && d.screens == 0 && d.name.empty());
    // Not its answer: another game's, a refusal, not JSON, not an object.
    CHECK(!parse_nlib_details(R"({"id":"0100000000020000","name":"x"})", 0x0100000000010000, &d));
    CHECK(!parse_nlib_details(R"({"success":false,"error":"Game not found"})", 0x0100000000010000, &d));
    CHECK(!parse_nlib_details("{\"id\":", 0x0100000000010000, &d));
    CHECK(!parse_nlib_details("[1]", 0x0100000000010000, &d));
    // Long text cut on a character, within its bound.
    const std::string long_text(9000, 'a');
    CHECK(parse_nlib_details("{\"id\":\"0100000000010000\",\"description\":\"" + long_text + "\"}", 0x0100000000010000, &d));
    CHECK(d.description.size() == 8192);
}
}   // namespace

int main()
{
    test_titles();
    test_icons();
    test_safe_text();
    test_manifest();
    test_list();
    test_nlib();
    return CHECK_DONE("title database: packs read, broken ones refused, safe text, manifests and their aliases, title lists, Nlib API's addresses and details passed");
}
