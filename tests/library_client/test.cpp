// library/client.cpp and library/servers.cpp against a scripted network: the
// address chosen and the credentials sent; the API, its older path, a JSON
// index and its directories (a cycle, the depth and fetch limits); a LAN
// server's own certificate (pin on first use offered, then used; a key
// that changed refused); a WAN one no authority vouches for; refusals. And
// the servers' file: written and read back, the password sealed, what a
// hand-edited file cannot hold dropped.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <algorithm>
#include <map>
#include <set>

#include "../library/vectors.hpp"
#include "check.h"
#include "fs.hpp"
#include "library/client.hpp"
#include "library/servers.hpp"
#include "temp_dir.hpp"

using namespace library;

namespace
{
struct Script : Transport
{
    std::map<std::string, Answer> answers;   // by URL; missing: 404
    std::set<std::string>         up;        // reachable hosts' URLs
    std::string                   pem;
    std::string                   expect_pin;   // a LAN request must carry it, else its TLS fails
    bool                          lan_own_cert = false;
    std::vector<std::string>      asked;
    std::vector<std::string>      last_headers;

    Answer get(const std::string& url, const std::vector<std::string>& headers, const std::string& pin) override
    {
        asked.push_back(url);
        last_headers = headers;
        if (lan_own_cert && url.compare(0, 19, "https://192.168.1.5") == 0 && pin != expect_pin) {
            Answer a;
            a.tls_failed = true;
            a.error = "SSL certificate problem";
            return a;
        }
        const auto it = answers.find(url);
        if (it != answers.end()) return it->second;
        Answer a;
        a.status = 404;
        return a;
    }
    bool reachable(const std::string& url) override { return up.count(url) != 0; }
    std::string certificate(const std::string&, std::string* why) override
    {
        if (pem.empty()) *why = "no certificate here";
        return pem;
    }
};

Answer ok(const std::string& body)
{
    Answer a;
    a.status = 200;
    a.body = body;
    return a;
}

const char* API = R"({"sections":[{"id":"all","title":"All","items":[{"url":"/api/get_game/7#Seven.nsp","size":7}]}]})";

Server shelf()
{
    Server s;
    s.id = "0123456789abcdef";
    s.name = "Shelf";
    s.lan_url = "https://192.168.1.5:8443/";
    s.wan_url = "https://shelf.example.org";
    s.username = "kid";
    return s;
}

void test_api_wan()
{
    Script t;
    t.answers["https://shelf.example.org/api/remote"] = ok(API);
    const Fetch f = fetch(shelf(), "secret", t);
    if (!CHECK(f.status == FetchStatus::Ok)) {
        fprintf(stderr, "  %s\n", f.why.c_str());
        return;
    }
    CHECK(f.endpoint == "https://shelf.example.org" && !f.on_lan && f.format == Format::Api);
    CHECK(f.catalog.items.size() == 1 && f.catalog.items[0].url == "https://shelf.example.org/api/get_game/7");
    CHECK(t.last_headers.size() == 1 && t.last_headers[0] == "Authorization: Basic a2lkOnNlY3JldA==");   // kid:secret
    // No user: no header.
    Server anon = shelf();
    anon.username.clear();
    fetch(anon, "", t);
    CHECK(t.last_headers.empty());
}

void test_lan_pin()
{
    Script t;
    t.up.insert("https://192.168.1.5:8443/");
    t.lan_own_cert = true;
    t.pem = CERT_EC_PEM;
    t.expect_pin = CERT_EC_PIN;
    t.answers["https://192.168.1.5:8443/api/remote"] = ok(API);
    Server s = shelf();
    // No pin yet: the key offered, nothing fetched.
    Fetch f = fetch(s, "secret", t);
    CHECK(f.status == FetchStatus::UnknownCertificate && f.on_lan);
    CHECK(f.offered_pin == CERT_EC_PIN && f.offered_short.size() == 23);
    // Trusted: the pin kept, the fetch goes through with it.
    s.pin = f.offered_pin;
    f = fetch(s, "secret", t);
    CHECK(f.status == FetchStatus::Ok && f.on_lan && f.catalog.items.size() == 1);
    // Another key than the one trusted: refused, never offered again.
    s.pin = CERT_RSA_PIN;
    f = fetch(s, "secret", t);
    CHECK(f.status == FetchStatus::Failed && f.why.find("no longer the one trusted") != std::string::npos);
    // The certificate cannot be read: said.
    t.pem.clear();
    s.pin.clear();
    f = fetch(s, "secret", t);
    CHECK(f.status == FetchStatus::Failed && f.why == "no certificate here");
    // Over the WAN, a certificate no authority vouches for is never pinned.
    Script w;
    Answer bad;
    bad.tls_failed = true;
    w.answers["https://shelf.example.org/api/remote"] = bad;
    f = fetch(shelf(), "", w);
    CHECK(f.status == FetchStatus::Failed && f.why.find("recognized authority") != std::string::npos);
    // Lan only: the LAN address even when it does not answer the probe.
    s = shelf();
    s.connection = Connection::Lan;
    s.pin = CERT_EC_PIN;
    t.up.clear();
    f = fetch(s, "", t);
    CHECK(f.status == FetchStatus::Ok && f.on_lan);
}

void test_fallbacks()
{
    // The older path.
    Script t;
    t.answers["https://shelf.example.org/api/shop"] = ok(API);
    Fetch f = fetch(shelf(), "", t);
    CHECK(f.status == FetchStatus::Ok && f.format == Format::Api);
    // Neither: the address read as an index, its directories followed (a
    // cycle once), merged.
    Script i;
    Server s = shelf();
    s.wan_url = "https://files.example.org/lib/index.json";
    i.answers[s.wan_url] = ok(R"({"files":["a.nsp"],"directories":["more.json","index.json"],"success":"hi"})");
    i.answers["https://files.example.org/lib/more.json"] = ok(R"({"files":["b.nsp","a.nsp"],"directories":["index.json"]})");
    f = fetch(s, "", i);
    CHECK(f.status == FetchStatus::Ok && f.format == Format::Index && f.catalog.motd == "hi");
    CHECK(f.catalog.items.size() == 2);
    // Its API paths tried first, then the index itself once (the cycle back to it not followed).
    CHECK(std::count(i.asked.begin(), i.asked.end(), s.wan_url) == 1);
    CHECK(std::count(i.asked.begin(), i.asked.end(), s.wan_url + "/api/remote") == 1);
    CHECK(std::count(i.asked.begin(), i.asked.end(), s.wan_url + "/api/shop") == 1);
    // A chain deeper than the limit stops there.
    Script deep;
    Server d = shelf();
    d.wan_url = "https://files.example.org/0.json";
    d.format = Format::Index;
    for (int n = 0; n < 10; n++)
        deep.answers["https://files.example.org/" + std::to_string(n) + ".json"] =
            ok(R"({"files":["f)" + std::to_string(n) + R"(.nsp"],"directories":[")" + std::to_string(n + 1) + R"(.json"]})");
    f = fetch(d, "", deep);
    CHECK(f.status == FetchStatus::Ok && f.catalog.items.size() == MAX_INDEX_DEPTH + 1);
    // Refusals.
    Script r;
    Answer denied;
    denied.status = 401;
    r.answers["https://shelf.example.org/api/remote"] = denied;
    f = fetch(shelf(), "wrong", r);
    CHECK(f.status == FetchStatus::Failed && f.why.find("username or password") != std::string::npos);
    Script none;
    Server api = shelf();
    api.format = Format::Api;
    f = fetch(api, "", none);
    CHECK(f.status == FetchStatus::Failed && f.why.find("404") != std::string::npos);
    Script junk;
    junk.answers["https://shelf.example.org/api/remote"] = ok("<html></html>");
    f = fetch(api, "", junk);
    CHECK(f.status == FetchStatus::Failed);
    Server no_https = shelf();
    no_https.lan_url.clear();
    no_https.wan_url = "http://plain.example.org";
    f = fetch(no_https, "", none);
    CHECK(f.status == FetchStatus::Failed && f.why.find("https") != std::string::npos);
}

void test_store()
{
    TempDir tmp("library_client", TempDir::Enter);
    Server  s = shelf();
    CHECK(set_password(s, "hunter2"));
    CHECK(s.password_box.compare(0, 3, "v1:") == 0 && s.password_box.find("hunter2") == std::string::npos);
    CHECK(password_of(s) == "hunter2");
    CHECK(pin_shown(s).empty());
    s.pin = CERT_EC_PIN;
    CHECK(pin_shown(s) == "B1:14:BD:D8:38:A3:A6:DE");
    s.connection = Connection::Lan;
    s.format = Format::Index;
    std::string why;
    CHECK(save_servers({s}, &why));
    {
        const std::vector<Server> list = {s};
        CHECK(find_server(list, s.id) == &list[0]);
        CHECK(find_server(list, "0000000000000000") == nullptr);
    }
    CHECK(fs::read(servers_file()).find("hunter2") == std::string::npos);
    const auto back = load_servers();
    if (!CHECK(back.size() == 1)) return;
    CHECK(back[0].id == s.id && back[0].name == "Shelf" && back[0].lan_url == s.lan_url && back[0].wan_url == s.wan_url);
    CHECK(back[0].connection == Connection::Lan && back[0].format == Format::Index && back[0].pin == CERT_EC_PIN);
    CHECK(password_of(back[0]) == "hunter2");
    CHECK(set_password(s, "") && s.password_box.empty() && password_of(s).empty());
    // What a hand-edited file cannot hold: http addresses, a plain password,
    // a pin of another kind, a server with no address or no id.
    const auto edited = servers_from_json(R"({"servers":[
        {"id":"a1","lan_url":"http://192.168.1.5","wan_url":"https://w","password":"plain","pin":"md5//x"},
        {"id":"a2","lan_url":"http://x"},
        {"lan_url":"https://y"},
        "junk"]})");
    CHECK(edited.size() == 1 && edited[0].lan_url.empty() && edited[0].wan_url == "https://w");
    CHECK(edited[0].password_box.empty() && edited[0].pin.empty());
    CHECK(servers_from_json("not json").empty());
    CHECK(new_server_id().size() == 16 && new_server_id() != new_server_id());
    // The editor's checks.
    Server e;
    CHECK(!check_server(e).empty());
    e.wan_url = "http://x";
    CHECK(check_server(e) == "https:// addresses only");
    e.wan_url = "https://x";
    CHECK(check_server(e).empty());
}
}   // namespace

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

int main()
{
    test_same_origin();
    test_api_wan();
    test_lan_pin();
    test_fallbacks();
    test_store();
    CHECK_DONE("library client: endpoints, credentials, API and index, pins on first use, the servers' file passed");
}
