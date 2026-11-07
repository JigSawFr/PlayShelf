// library/servers.cpp: the servers' file written and read back (their
// uid, the connection), what a hand-edited file cannot hold dropped; its
// schema (none counts as 1, whose servers are carried over once without the
// other protocols' credentials, a newer one is read as none and never
// written over) and the keys it does not know.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <algorithm>

#include "check.h"
#include "fs.hpp"
#include "library/servers.hpp"
#include "temp_dir.hpp"

using namespace library;

namespace
{
void test_round_trip()
{
    Server a;
    a.id = "a1";
    a.name = "Living room";
    a.lan_url = "https://192.168.1.5:8443";
    a.wan_url = "https://shelf.example.org";
    a.connection = Connection::Lan;
    a.uid = "6f1c2a8e-4b7d-4e2a-9c31-8d5e2f0a7b14";
    Server b;
    b.id = "b2";
    b.wan_url = "https://den.example.org";

    const std::string text = servers_to_json({ a, b });
    const auto        back = servers_from_json(text);
    if (!CHECK(back.size() == 2)) return;
    CHECK(back[0].id == "a1" && back[0].name == "Living room" && back[0].lan_url == a.lan_url && back[0].wan_url == a.wan_url);
    CHECK(back[0].connection == Connection::Lan && back[0].uid == a.uid);
    CHECK(back[1].uid.empty() && back[1].connection == Connection::Auto);
    CHECK(text.find("\"schema\": 2") != std::string::npos);

    // A hand-edited file: an address that is not https, a server with none left, no id.
    const auto edited = servers_from_json(R"({"schema":2,"servers":[
        {"id":"c3","lan_url":"http://plain.example.org","wan_url":"https://ok.example.org"},
        {"id":"d4","lan_url":"http://only-plain.example.org"},
        {"name":"no id","wan_url":"https://x.example.org"}]})");
    CHECK(edited.size() == 1 && edited[0].lan_url.empty() && edited[0].wan_url == "https://ok.example.org");
    CHECK(servers_from_json("not json").empty());
    CHECK(new_server_id().size() == 16 && new_server_id() != new_server_id());
}

void test_schema()
{
    TempDir tmp("library_servers", TempDir::Enter);
    const std::string server = R"({"id":"b2","name":"Den","wan_url":"https://den.example.org")";

    // Schema 1, with what it held for the other protocols: carried over once.
    const std::string old = R"({"schema":1,"mirror":{"on":true},"servers":[)" + server +
                            R"(,"format":"api","username":"kid","password":"v1:sealed","pin":"sha256//abc","shelf":"top"}]})";
    fs::write(servers_file(), old);
    CHECK(servers_old(old));
    auto list = load_servers();
    if (!CHECK(list.size() == 1)) return;
    CHECK(list[0].name == "Den" && list[0].uid.empty());   // it pairs again
    const std::string text = fs::read(servers_file());
    CHECK(!servers_old(text) && text.find("\"schema\": 2") != std::string::npos);
    for (const char* gone : {"username", "password", "sealed", "sha256//abc", "\"format\"", "\"pin\""})
        CHECK(text.find(gone) == std::string::npos);
    CHECK(text.find("\"shelf\": \"top\"") != std::string::npos && text.find("\"mirror\"") != std::string::npos);

    // Keys this build does not know, in the file and in a server, are written back.
    list[0].name = "Attic";
    std::string why;
    CHECK(save_servers(list, &why));
    const std::string saved = fs::read(servers_file());
    CHECK(saved.find("\"Attic\"") != std::string::npos && saved.find("\"shelf\": \"top\"") != std::string::npos &&
          saved.find("\"mirror\"") != std::string::npos);

    // A newer file: nothing read, the save refused, the file as it was.
    const std::string newer = R"({"schema":3,"servers":[)" + server + "}]}";
    fs::write(servers_file(), newer);
    CHECK(servers_newer(newer) && !servers_newer(saved));
    CHECK(load_servers().empty());
    CHECK(!save_servers(list, &why) && !why.empty());
    CHECK(fs::read(servers_file()) == newer);
}

void test_find()
{
    Server a;
    a.id = "a1";
    const std::vector<Server> all{ a };
    CHECK(find_server(all, "a1") == &all[0] && !find_server(all, "zz"));
    Server none;
    CHECK(!check_server(none).empty());
    none.wan_url = "http://x";
    CHECK(check_server(none) == "https:// addresses only");
    none.wan_url = "https://x";
    CHECK(check_server(none).empty());
}
}   // namespace

int main()
{
    test_round_trip();
    test_schema();
    test_find();
    CHECK_DONE("library servers: file written and read back, schema 1 carried over once, newer schema untouched passed");
}
