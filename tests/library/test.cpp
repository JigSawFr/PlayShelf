// source/library/: the catalogue of an AeroFoil/Ownfoil API answer and of a
// JSON index (addresses made absolute and https only, names, ids, versions,
// kinds, sections, the MOTD, the titledb, what is left out), address
// resolution, title id kinds, LAN discovery answers, the endpoint chosen,
// and certificate pins against another implementation's (vectors.hpp).
// With PLAYSHELF_WRITE_SEEDS=<dir>, the answers are written there as the
// fuzzers' seeds (tests/fuzz/library_*/corpus).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "check.h"
#include "library/catalog.hpp"
#include "library/server.hpp"
#include "vectors.hpp"

using namespace library;

namespace
{
void write_seed(const char* name, const std::string& s)
{
    const char* dir = std::getenv("PLAYSHELF_WRITE_SEEDS");
    if (!dir) return;
    std::ofstream(std::string(dir) + "/" + name, std::ios::binary) << s;
}

const char* API = R"({
  "motd": "Welcome to the shelf\nSecond line",
  "sections": [
    {"id": "new", "title": "New", "items": [
      {"name": "Game One", "url": "/api/get_game/1#Game%20One.nsp", "size": 1234567, "title_id": "0100AAAA00010000",
       "app_version": 0, "app_type": "BASE", "icon_url": "/api/remote/icon/0100AAAA00010000", "release_date": "2026-03-04"},
      {"url": "files/Game%20One%20[0100AAAA00010800][v131072].nsp", "size": 2000},
      {"name": "A cheat", "url": "/cheats/x.txt", "app_type": "CHEAT"}
    ]},
    {"id": "dlc", "title": "DLC", "items": [
      {"url": "https://cdn.example.org/dlc/pack.nsz", "title_id": 81064793292673025, "app_type": 130},
      {"url": "http://plain.example.org/x.nsp"},
      {"url": "gdrive:/1AbCdE#x.nsp"},
      {"url": "/api/get_game/1#Game%20One.nsp"},
      {"size": 3}
    ]},
    {"id": "cheats", "title": "Cheats", "items": [{"url": "/c.txt"}]}
  ]
})";

void test_api()
{
    write_seed("api", API);
    std::string why;
    const auto  c = parse_api(API, "https://shelf.example.org:8443/api/remote", &why);
    if (!CHECK(c)) {
        fprintf(stderr, "  %s\n", why.c_str());
        return;
    }
    CHECK(c->motd == "Welcome to the shelf Second line");   // a control character made a space
    CHECK(c->sections.size() == 3);
    CHECK(c->sections[0].id == "new" && c->sections[0].title == "New" && c->sections[0].items.size() == 2);
    CHECK(c->sections[1].items.size() == 1);
    CHECK(c->sections[2].items.empty());   // cheats: no package
    CHECK(c->items.size() == 3);
    const Item& g = c->items[0];
    CHECK(g.name == "Game One");
    CHECK(g.url == "https://shelf.example.org:8443/api/get_game/1");
    CHECK(g.size == 1234567 && g.title_id == 0x0100AAAA00010000ull && g.has_version && g.version == 0);
    CHECK(g.kind == Kind::Base && g.release_date == 20260304 && g.section == "new");
    CHECK(g.icon_url == "https://shelf.example.org:8443/api/remote/icon/0100AAAA00010000");
    const Item& u = c->items[1];
    CHECK(u.url == "https://shelf.example.org:8443/api/files/Game%20One%20[0100AAAA00010800][v131072].nsp");
    CHECK(u.name == "Game One [0100AAAA00010800][v131072].nsp");
    CHECK(u.title_id == 0x0100AAAA00010800ull && u.version == 131072 && u.kind == Kind::Update);
    const Item& d = c->items[2];
    CHECK(d.url == "https://cdn.example.org/dlc/pack.nsz" && d.name == "pack.nsz");
    CHECK(d.title_id == 0x0120000000001001ull && d.kind == Kind::Dlc);
    CHECK(c->skipped == 4);   // http, gdrive, the same address again, no address

    // The server's own refusal, and answers that are no catalogue.
    CHECK(!parse_api(R"({"error": "Unauthorized: wrong password"})", "https://a/", &why) && why == "Unauthorized: wrong password");
    CHECK(!parse_api(R"({"success": "hi"})", "https://a/", &why) && why.find("no sections") != std::string::npos);
    CHECK(!parse_api("<html>login</html>", "https://a/", &why));
    CHECK(!parse_api("[1,2]", "https://a/"));
    std::string deep = std::string(100, '[') + std::string(100, ']');
    CHECK(!parse_api(R"({"sections": )" + deep + "}", "https://a/"));
    // A server over http: nothing of it is kept.
    const auto plain = parse_api(R"({"sections": [{"id": "a", "items": [{"url": "/x.nsp"}]}]})", "http://a/");
    CHECK(plain && plain->items.empty() && plain->skipped == 1);
}

const char* INDEX = R"({
  "files": [
    "https://files.example.org/games/Alpha%20[0100DDDD00010000][v0].nsp",
    {"url": "Beta.xci", "size": 777},
    {"url": "../up/Gamma [0100CCCC00010800][v65536].nsz", "name": "Gamma update"}
  ],
  "directories": ["more/index.json", "https://other.example.org/i.json", "http://x/i.json", "more/index.json"],
  "titledb": {"0100BBBB00010000": {"name": "Beta Quest", "version": 0}, "zz": {"name": "bad"}, "0100CCCC00010000": {"name": "Gamma"}},
  "success": "Index of the week"
})";

void test_index()
{
    write_seed("index", INDEX);
    const auto c = parse_index(INDEX, "https://files.example.org/lib/index.json");
    if (!CHECK(c)) return;
    CHECK(c->motd == "Index of the week");
    CHECK(c->items.size() == 3 && c->sections.empty());
    CHECK(c->items[0].name == "Alpha [0100DDDD00010000][v0].nsp" && c->items[0].title_id == 0x0100DDDD00010000ull);
    CHECK(c->items[1].url == "https://files.example.org/lib/Beta.xci" && c->items[1].size == 777);
    CHECK(c->items[2].url == "https://files.example.org/up/Gamma%20[0100CCCC00010800][v65536].nsz" ||
          c->items[2].url == "https://files.example.org/up/Gamma [0100CCCC00010800][v65536].nsz");
    CHECK(c->items[2].name == "Gamma update" && c->items[2].kind == Kind::Update && c->items[2].version == 65536);
    CHECK(c->directories.size() == 2);
    CHECK(c->directories[0] == "https://files.example.org/lib/more/index.json");
    CHECK(c->titles.size() == 2 && c->titles.at(0x0100BBBB00010000ull).name == "Beta Quest");
    CHECK(!parse_index(R"({"other": 1})", "https://a/"));
    CHECK(!parse_index("not json", "https://a/"));
    CHECK(parse_index(R"({"success": "only a message"})", "https://a/"));
}

void test_resolve()
{
    const std::string base = "https://h.example:8443/a/b/index.json?k=v";
    CHECK(resolve(base, "c.nsp") == "https://h.example:8443/a/b/c.nsp");
    CHECK(resolve(base, "./c.nsp") == "https://h.example:8443/a/b/c.nsp");
    CHECK(resolve(base, "../c.nsp") == "https://h.example:8443/a/c.nsp");
    CHECK(resolve(base, "../../../../c.nsp") == "https://h.example:8443/c.nsp");
    CHECK(resolve(base, "/x/y?z=1") == "https://h.example:8443/x/y?z=1");
    CHECK(resolve(base, "//cdn.example/f") == "https://cdn.example/f");
    CHECK(resolve(base, "HTTPS://Other/f") == "https://Other/f");
    CHECK(resolve(base, "http://other/f") == "");
    CHECK(resolve(base, "ftp://other/f") == "");
    CHECK(resolve(base, "javascript:alert(1)") == "");
    CHECK(resolve("https://h.example", "c") == "https://h.example/c");
    CHECK(resolve("http://h.example/", "c") == "");
    CHECK(resolve("https://", "c") == "");
    CHECK(resolve(base, "") == "");
}

void test_titles()
{
    CHECK(kind_of(0x0100AAAA00010000ull) == Kind::Base);
    CHECK(kind_of(0x0100AAAA00010800ull) == Kind::Update);
    CHECK(kind_of(0x0100AAAA00011001ull) == Kind::Dlc);
    CHECK(kind_of(0x0100AAAA00010123ull) == Kind::Unknown);
    CHECK(kind_of(0) == Kind::Unknown);
    CHECK(base_of(0x0100AAAA00010800ull) == 0x0100AAAA00010000ull);
    CHECK(base_of(0x0100AAAA00011001ull) == 0x0100AAAA00010000ull);
    CHECK(base_of(0x0100AAAA00011fffull) == 0x0100AAAA00010000ull);
    uint64_t tid = 0;
    uint32_t v = 0;
    bool     hv = false;
    CHECK(parse_name_tags("Z [0100AAAA00010800][v65536] (x).nsz", &tid, &v, &hv) && tid == 0x0100AAAA00010800ull && hv && v == 65536);
    CHECK(!parse_name_tags("Z [v3]", &tid, &v, &hv) && hv && v == 3);
    CHECK(!parse_name_tags("Z [0100AAAA0001000] [vx] [v99999999999]", &tid, &v, &hv) && !hv);
    // Display text: bounded without cutting a character, controls made spaces.
    CHECK(display_text("a\tb\x7f", 10) == "a b ");
    CHECK(display_text("\xc3\xa9t\xc3\xa9", 4) == "\xc3\xa9t");
    CHECK(display_text("ab\xe2\x82", 10) == "ab\xef\xbf\xbd\xef\xbf\xbd");   // an unfinished sequence replaced
    CHECK(display_text("a\xff\xc0\xaf" "b", 20) == "a\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd" "b");   // invalid, overlong
    CHECK(display_text("\xed\xa0\x80", 20) == "\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd");   // a surrogate
}

void test_discovery()
{
    const std::string ok = R"({"magic":"OWNFOIL","uid":"0A1B2C3D-0000-1111-2222-333344445555","name":"Shelf\nServer","version":"2.4","port":8443,"public":false,"remote":"https://shelf.example.org"})";
    write_seed("discovery", ok);
    const auto s = parse_discovery(ok.data(), ok.size(), "192.168.1.20");
    if (!CHECK(s)) return;
    CHECK(s->uid == "0a1b2c3d-0000-1111-2222-333344445555" && s->name == "Shelf Server" && s->port == 8443);
    CHECK(!s->is_public && s->remote == "https://shelf.example.org" && s->url() == "https://192.168.1.20:8443");
    CHECK(!parse_discovery(ok.data(), ok.size(), "255.255.255.255"));
    CHECK(!parse_discovery(ok.data(), ok.size(), "224.0.0.1"));
    CHECK(!parse_discovery(ok.data(), ok.size(), "0.0.0.0"));
    CHECK(!parse_discovery(ok.data(), ok.size(), "not.an.ip"));
    for (const char* bad : {"192.168.1", "192.168.1.256", "192.168.1.2.3", " 192.168.1.2", "192.168.1.2 ", "-1.2.3.4", "1234.1.1.1"})
        CHECK(!parse_discovery(ok.data(), ok.size(), bad));
    const std::string http_remote = R"({"magic":"OWNFOIL","uid":"0a1b2c3d-0000-1111-2222-333344445555","name":"a","version":"1","port":80,"public":true,"remote":"http://x"})";
    const auto h = parse_discovery(http_remote.data(), http_remote.size(), "10.0.0.2");
    CHECK(h && h->remote.empty());
    // Ownfoil's host alone: over https; anything else in it, dropped.
    const std::string bare = R"({"magic":"OWNFOIL","uid":"0a1b2c3d-0000-1111-2222-333344445555","name":"a","version":"1","port":8465,"public":true,"remote":"shop.example.org"})";
    const auto b = parse_discovery(bare.data(), bare.size(), "10.0.0.2");
    CHECK(b && b->remote == "https://shop.example.org");
    const std::string odd = R"({"magic":"OWNFOIL","uid":"0a1b2c3d-0000-1111-2222-333344445555","name":"a","version":"1","port":8465,"public":true,"remote":"shop example/x"})";
    const auto o = parse_discovery(odd.data(), odd.size(), "10.0.0.2");
    CHECK(o && o->remote.empty());
    // Ownfoil 2's handshake.
    const auto info = parse_handshake(R"({"uid":"u","name":"Home\nShop","version":"2.5.0","protocol_version":1,"motd":"Hi","public":true,"features":{"save_backup":true}})");
    CHECK(info && info->name == "Home Shop" && info->motd == "Hi" && info->protocol == 1 && info->public_shop && info->save_backup);
    CHECK(!parse_handshake("<html>") && !parse_handshake(R"({"name":"no uid"})"));
    CHECK(url_encode("{a b&c}") == "%7Ba%20b%26c%7D");
    for (const char* bad : {R"({"magic":"OTHER","uid":"0a1b2c3d-0000-1111-2222-333344445555","name":"a","version":"1","port":80,"public":true})",
                            R"({"magic":"OWNFOIL","uid":"not-a-uuid","name":"a","version":"1","port":80,"public":true})",
                            R"({"magic":"OWNFOIL","uid":"0a1b2c3d-0000-1111-2222-333344445555","name":"a","version":"1","port":70000,"public":true})",
                            R"({"magic":"OWNFOIL","uid":"0a1b2c3d-0000-1111-2222-333344445555","name":"a","version":"1","port":80})",
                            "[]", "{"})
        CHECK(!parse_discovery(bad, std::strlen(bad), "10.0.0.2"));
    std::vector<Discovered> list;
    add_discovered(list, *s);
    Discovered again = *s;
    again.name = "Renamed";
    add_discovered(list, again);
    CHECK(list.size() == 1 && list[0].name == "Renamed");
}

void test_endpoint()
{
    int        asked = 0;
    const auto yes = [&](const std::string&) { return ++asked > 0; };
    const auto no = [&](const std::string&) {
        ++asked;
        return false;
    };
    Endpoints e{"https://192.168.1.20:8443", "https://shelf.example.org", Connection::Auto};
    CHECK(choose_endpoint(e, yes) == e.lan_url);
    CHECK(choose_endpoint(e, no) == e.wan_url);
    e.connection = Connection::Lan;
    asked = 0;
    CHECK(choose_endpoint(e, no) == e.lan_url && asked == 0);
    e.connection = Connection::Wan;
    CHECK(choose_endpoint(e, yes) == e.wan_url && asked == 0);
    CHECK(choose_endpoint(Endpoints{"", "https://w", Connection::Lan}, yes) == "https://w");
    CHECK(choose_endpoint(Endpoints{"https://l", "", Connection::Wan}, yes) == "https://l");
}

void test_pins()
{
    for (const auto& [der, size, pem, pin] :
         {std::make_tuple(CERT_EC, sizeof CERT_EC, CERT_EC_PEM, CERT_EC_PIN), std::make_tuple(CERT_RSA, sizeof CERT_RSA, CERT_RSA_PEM, CERT_RSA_PIN)}) {
        const auto h = spki_sha256(der, size);
        if (!CHECK(h)) continue;
        CHECK(pin_text(*h) == pin);
        const auto from_pem = pem_to_der(pem);
        CHECK(from_pem && from_pem->size() == size && std::memcmp(from_pem->data(), der, size) == 0);
        CHECK(pin_short(*h).size() == 8 * 3 - 1);
        // Cut, or with a byte more: no certificate.
        CHECK(!spki_sha256(der, size - 1));
        std::vector<uint8_t> longer(der, der + size);
        longer.push_back(0);
        CHECK(!spki_sha256(longer.data(), longer.size()));
        for (size_t n = 0; n < size; n += 37)
            CHECK(!spki_sha256(der, n));
    }
    CHECK(base64((const uint8_t*)"Man", 3) == "TWFu" && base64((const uint8_t*)"Ma", 2) == "TWE=" && base64((const uint8_t*)"M", 1) == "TQ==");
    const auto back = unbase64("TWFu\nTWE=");
    CHECK(!unbase64("TWE=x") && !unbase64("T!") && back);
    CHECK(!pem_to_der("no certificate here"));
}
}   // namespace

int main()
{
    test_api();
    test_index();
    test_resolve();
    test_titles();
    test_discovery();
    test_endpoint();
    test_pins();
    CHECK_DONE("library: API and index catalogues, addresses, title ids, discovery, endpoints and pins passed");
}
