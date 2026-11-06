// source/lantern/credential_store.cpp: what a paired server keeps (token,
// device key, pinned key) sealed in lantern.json, read back by `uid`; the
// file absent, from a newer schema (read as absent, never written over), a
// key this build does not know (kept), a sealed value that cannot be opened.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "fs.hpp"
#include "lantern/credential_store.hpp"
#include "temp_dir.hpp"
#include "util/paths.hpp"

#include <aurore/extern/nlohmann/json.hpp>

using namespace lantern_app;
using nlohmann::json;

namespace
{
lantern::Credentials sample()
{
    lantern::Credentials c;
    c.token = "tok-secret-1";
    lantern::KeySeed seed{};
    for (size_t i = 0; i < seed.size(); i++)
        seed[i] = (uint8_t)(i * 7 + 1);
    c.key = seed;
    lantern::Digest spki{};
    for (size_t i = 0; i < spki.size(); i++)
        spki[i] = (uint8_t)(255 - i);
    c.spki = spki;
    return c;
}

std::string read(const std::string& path)
{
    std::string t;
    paths::read_file(path, t);
    return t;
}

void test_round_trip()
{
    TempDir tmp("lantern_store", TempDir::Enter);
    FileCredentialStore store(tmp.path() + "/lantern.json");

    auto none = store.load("srv-a", {});
    CHECK(none && !*none);   // absent file: not paired

    const auto c = sample();
    CHECK(store.store("srv-a", c).has_value());
    CHECK(store.store("srv-b", c).has_value());

    auto back = store.load("srv-a", {});
    CHECK(back && *back);
    if (back && *back) {
        CHECK((*back)->token == "tok-secret-1");
        CHECK((*back)->key == c.key);
        CHECK((*back)->spki == c.spki);
    }

    // The token and the seed are on the SD card only sealed.
    const std::string text = read(tmp.path() + "/lantern.json");
    CHECK(text.find("tok-secret-1") == std::string::npos);
    CHECK(json::parse(text).at("schema") == 1);

    CHECK(store.erase("srv-a").has_value());
    auto gone = store.load("srv-a", {});
    CHECK(gone && !*gone);
    auto other = store.load("srv-b", {});
    CHECK(other && *other);
    CHECK(store.erase("never-paired").has_value());
}

void test_without_key_or_pin()
{
    TempDir tmp("lantern_store", TempDir::Enter);
    FileCredentialStore store(tmp.path() + "/lantern.json");
    lantern::Credentials c;
    c.token = "bearer-only";
    CHECK(store.store("srv", c).has_value());
    auto back = store.load("srv", {});
    CHECK(back && *back && !(*back)->key && !(*back)->spki && (*back)->token == "bearer-only");
}

void test_newer_schema()
{
    TempDir tmp("lantern_store", TempDir::Enter);
    const std::string path = tmp.path() + "/lantern.json";
    const std::string newer = R"({"schema":2,"servers":{"srv":{"token":"v1:x"}}})";
    CHECK(paths::atomic_write(path, newer));
    FileCredentialStore store(path);
    auto r = store.load("srv", {});
    CHECK(r && !*r);                                  // read as absent, no error
    CHECK(!store.store("srv", sample()).has_value()); // never written over
    CHECK(!store.erase("srv").has_value());
    CHECK(read(path) == newer);
}

void test_unknown_keys_and_unreadable()
{
    TempDir tmp("lantern_store", TempDir::Enter);
    const std::string path = tmp.path() + "/lantern.json";
    FileCredentialStore store(path);
    CHECK(store.store("srv", sample()).has_value());

    json root = json::parse(read(path));
    root["future"] = "kept";
    root["servers"]["srv"]["note"] = "kept too";
    CHECK(paths::atomic_write(path, root.dump()));
    CHECK(store.store("srv", sample()).has_value());
    const json after = json::parse(read(path));
    CHECK(after.at("future") == "kept" && after.at("servers").at("srv").at("note") == "kept too");

    // A token that is not sealed for this console is no token.
    root = json::parse(read(path));
    root["servers"]["srv"]["token"] = "plain";
    CHECK(paths::atomic_write(path, root.dump()));
    auto r = store.load("srv", {});
    CHECK(r && !*r);

    // A file that is not JSON is an empty one.
    CHECK(paths::atomic_write(path, "not json"));
    r = store.load("srv", {});
    CHECK(r && !*r);
    CHECK(store.store("srv", sample()).has_value());
}
}   // namespace

int main()
{
    test_round_trip();
    test_without_key_or_pin();
    test_newer_schema();
    test_unknown_keys_and_unreadable();
    CHECK_DONE("lantern store: sealed round trip, newer schema untouched, unknown keys kept passed");
}
