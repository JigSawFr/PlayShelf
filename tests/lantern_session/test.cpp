// source/lantern_app/session.cpp over a scripted network: the address chosen
// (LAN, else WAN), a server not paired yet, a paired one read page by page
// (items and sections), another server answering at a known address.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <functional>
#include <map>

#include "check.h"
#include "lantern_app/session.hpp"

using namespace lantern_app;

namespace
{
struct Reply
{
    uint16_t    status = 200;
    std::string body;
    bool        down = false;   // no connection at all
};

struct Net final : lantern::Transport
{
    std::function<Reply(const std::string& url)> respond;
    std::vector<std::string>                     asked;
    std::vector<std::string>                     bearer;

    lantern::Result<void> send(const lantern::Request& r, lantern::ResponseHandler& h) override
    {
        const std::string url(r.url);
        asked.push_back(url);
        for (const auto& hd : r.headers)
            if (hd.name == "Authorization") bearer.push_back(std::string(hd.value));
        const Reply a = respond(url);
        if (a.down) return lantern::fail(lantern::Category::transport, "connection refused");
        if (!h.on_status(a.status)) return {};
        if (!h.on_header("Content-Type", "application/json")) return {};
        const std::span<const std::byte> bytes(reinterpret_cast<const std::byte*>(a.body.data()), a.body.size());
        if (!h.on_body(bytes)) return {};
        return {};
    }
};

struct Random final : lantern::RandomSource
{
    lantern::Result<void> fill(std::span<std::uint8_t> out) override
    {
        for (size_t i = 0; i < out.size(); i++) out[i] = (uint8_t)(i * 31 + 7);
        return {};
    }
};

struct FixedClock final : lantern::Clock
{
    std::int64_t unix_seconds() override { return 1'800'000'000; }
};

struct Store final : lantern::CredentialStore
{
    std::map<std::string, lantern::Credentials> kept;
    lantern::Result<std::optional<lantern::Credentials>> load(std::string_view uid, lantern::Allocator) override
    {
        const auto it = kept.find(std::string(uid));
        if (it == kept.end()) return std::optional<lantern::Credentials>{};
        return std::optional<lantern::Credentials>{it->second};
    }
    lantern::Result<void> store(std::string_view uid, const lantern::Credentials& c) override { kept[std::string(uid)] = c; return {}; }
    lantern::Result<void> erase(std::string_view uid) override { kept.erase(std::string(uid)); return {}; }
};

std::string shake(const char* uid)
{
    return std::string(R"({"schema":1,"uid":")") + uid + R"(","name":"Living room shelf","base":"/api/lantern","versions":[1],
      "platforms":["switch"],"motd":"Welcome","account":{"state":"active","scopes":["library"]},
      "features":{"library":1,"titles":1,"changes":1,"sections":1,"homebrew":0,"saves":0,"cheats":0,"requests":0,"pairing":1}})";
}

std::string app_json(const char* id, const char* app_id, const char* type, int version, const char* name)
{
    return std::string(R"({"id":")") + id + R"(","platform":"switch","title_id":"0100ABCD00010000","app_id":")" + app_id
           + R"(","type":")" + type + R"(","version_code":)" + std::to_string(version) + R"(,"name":")" + name
           + R"(","size":4096,"format":"nsp","sha256":")" + std::string(64, 'a') + R"(","file":"/files/)" + id + R"(.nsp"})";
}

const std::string PAGE1 = R"({"schema":1,"items":[)" + app_json("a1", "0100ABCD00010000", "base", 0, "Tide Runner") + ","
                          + app_json("a2", "0100ABCD00010800", "update", 65536, "Tide Runner") + R"(],"next":"c2"})";
const std::string PAGE2 = R"({"schema":1,"items":[)" + app_json("a3", "0100ABCD00011001", "dlc", 0, "Tide Runner Pack") + R"(],"next":null})";
const std::string SECTIONS = R"({"schema":1,"sections":[{"id":"new","title":"New","apps":["a3","a1"]}]})";

const std::string TITLES = R"({"schema":1,"titles":[{"platform":"switch","title_id":"0100abcd00010000","language":"en","name":"Tide Runner",
  "publisher":"Example Games","release":"2024-03-08","updated":"2026-10-30T09:12:03.250Z","genres":["Adventure"],"icon":{"url":"/icons/a.jpg"}}],"unknown":[]})";

Reply lantern_server(const std::string& url, const char* uid, const std::string& host)
{
    if (url.find(host) == std::string::npos) return {404, "", true};
    if (url.ends_with("/.well-known/lantern")) return {200, shake(uid)};
    if (url.find("/v1/apps?") != std::string::npos) return {200, url.find("cursor=c2") != std::string::npos ? PAGE2 : PAGE1};
    if (url.ends_with("/v1/sections")) return {200, SECTIONS};
    if (url.find("/v1/titles?ids=0100abcd00010000") != std::string::npos) return {200, TITLES};
    return {404, R"({"status":404})"};
}

struct World
{
    Net         net;
    Random      random;
    FixedClock  clock;
    Store       store;
    lantern::ServerTrust trust;
    Env env{ net, trust, random, clock, "PlayShelf/test", "switch", "" };
};

library::Server server(const char* lan, const char* wan, const char* uid)
{
    library::Server s;
    s.id = "0123456789abcdef";
    s.name = "Shelf";
    s.lan_url = lan;
    s.wan_url = wan;
    s.uid = uid;
    return s;
}

void test_choose()
{
    World w;
    w.net.respond = [](const std::string& url) { return lantern_server(url, "u-1", "wan.example.org"); };
    // The LAN address does not answer: the WAN one.
    CHECK(choose(server("https://192.168.1.5:8443", "https://wan.example.org", "u-1"), w.env, w.store) == "https://wan.example.org");
    w.net.respond = [](const std::string& url) { return lantern_server(url, "u-1", "192.168.1.5"); };
    CHECK(choose(server("https://192.168.1.5:8443/", "https://wan.example.org", "u-1"), w.env, w.store) == "https://192.168.1.5:8443");
}

void test_not_paired()
{
    World w;
    w.net.respond = [](const std::string& url) { return lantern_server(url, "u-1", "wan.example.org"); };
    const Fetched f = fetch(server("", "https://wan.example.org", ""), w.env, w.store);
    CHECK(f.status == Status::NotPaired && f.name == "Living room shelf" && f.uid == "u-1");
    CHECK(f.catalog.items.empty());
}

void test_paired()
{
    World w;
    w.net.respond = [](const std::string& url) { return lantern_server(url, "u-1", "wan.example.org"); };
    lantern::Credentials c;
    c.token = "et_fictional";
    w.store.kept["u-1"] = c;
    const Fetched f = fetch(server("", "https://wan.example.org", "u-1"), w.env, w.store);
    CHECK(f.status == Status::Ok && !f.on_lan && f.endpoint == "https://wan.example.org");
    CHECK(f.catalog.items.size() == 3 && f.catalog.motd == "Welcome");
    CHECK(f.catalog.items[1].kind == library::Kind::Update && f.catalog.items[1].version == 65536);
    CHECK(f.catalog.items[0].url == "https://wan.example.org/files/a1.nsp");
    CHECK(f.catalog.sections.size() == 1 && f.catalog.sections[0].items == std::vector<size_t>({ 2, 0 }));
    CHECK(!w.net.bearer.empty() && w.net.bearer.back() == "Bearer et_fictional");
    // The sheet of the game, asked once for its three items; its icon on all.
    CHECK(f.titles.size() == 1 && f.titles[0].base == 0x0100ABCD00010000ULL && f.titles[0].facts.publisher == "Example Games");
    CHECK(f.titles[0].facts.release_date == 20240308 && f.catalog.items[0].icon_url == "https://wan.example.org/icons/a.jpg");
}

void test_other_server()
{
    World w;
    w.net.respond = [](const std::string& url) { return lantern_server(url, "someone-else", "wan.example.org"); };
    lantern::Credentials c;
    c.token = "et_fictional";
    w.store.kept["u-1"] = c;
    const Fetched f = fetch(server("", "https://wan.example.org", "u-1"), w.env, w.store);
    CHECK(f.status == Status::Failed && f.why.find("another server") != std::string::npos);
    CHECK(w.net.bearer.empty());   // the token was never sent to it
}

void test_down_and_no_address()
{
    World w;
    w.net.respond = [](const std::string&) { return Reply{0, "", true}; };
    CHECK(fetch(server("", "https://wan.example.org", "u-1"), w.env, w.store).status == Status::Failed);
    library::Server s = server("", "", "u-1");
    CHECK(fetch(s, w.env, w.store).why == "the server has no https:// address");
}
}   // namespace

int main()
{
    test_choose();
    test_not_paired();
    test_paired();
    test_other_server();
    test_down_and_no_address();
    CHECK_DONE("lantern session: address choice, not paired, paged apps and sections, another server refused passed");
}
