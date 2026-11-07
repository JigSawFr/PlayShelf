// source/library/: address resolution, title id kinds, the text a screen
// shows of a server's, which address of a server is chosen, and whether two
// addresses are of one origin.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <string>

#include "check.h"
#include "library/catalog.hpp"
#include "library/server.hpp"

using namespace library;

namespace
{
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
    // Display text: bounded without cutting a character, controls made spaces.
    CHECK(display_text("a\tb\x7f", 10) == "a b ");
    CHECK(display_text("\xc3\xa9t\xc3\xa9", 4) == "\xc3\xa9t");
    CHECK(display_text("ab\xe2\x82", 10) == "ab\xef\xbf\xbd\xef\xbf\xbd");   // an unfinished sequence replaced
    CHECK(display_text("a\xff\xc0\xaf" "b", 20) == "a\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd" "b");   // invalid, overlong
    CHECK(display_text("\xed\xa0\x80", 20) == "\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd");   // a surrogate
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

void test_same_origin()
{
    CHECK(same_origin("https://shelf.example/api/remote", "https://SHELF.example:443/files/a.nsp"));
    CHECK(same_origin("https://192.168.1.5:8465/", "https://192.168.1.5:8465/files/a.nsp?x=1"));
    CHECK(!same_origin("https://192.168.1.5:8465/", "https://192.168.1.5/files/a.nsp"));
    CHECK(!same_origin("https://shelf.example/", "https://shelf.example.evil/a.nsp"));
    CHECK(!same_origin("https://shelf.example/", "https://evil@other.example/a.nsp"));
    CHECK(same_origin("https://shelf.example/", "https://me@shelf.example/a.nsp"));
    CHECK(!same_origin("https://shelf.example/", "http://shelf.example/a.nsp"));
    CHECK(!same_origin("", ""));
}
}   // namespace

int main()
{
    test_resolve();
    test_titles();
    test_endpoint();
    test_same_origin();
    CHECK_DONE("library: addresses, title ids, endpoints and origins passed");
}
