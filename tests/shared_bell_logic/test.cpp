// shared_bell_logic: PlayShelf's bell as the other Boréale homebrews read it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstdlib>
#include <string>

#include "action/shared_bell_logic.hpp"
#include "check.h"
#include "temp_dir.hpp"

using shared_bell_logic::Entry;
using shared_bell_logic::text;

static bool has(const std::string& s, const std::string& part)
{
    return s.find(part) != std::string::npos;
}

static void test_written()
{
    const std::vector<Entry> entries = {
        { "pin_missing", true, "No PIN yet", "Anyone can change the limits.", "security" },
        { "week", false, "Your week", "12 h played", "activity" },
        { "odd", true, "Odd", "", "Not A Route" },
    };
    const std::string t = text(entries, "en", "/switch/PlayShelf/PlayShelf.nro", "", 1000);
    CHECK(has(t, "\"schema\":1") && has(t, "\"module\":\"playshelf\"") && has(t, "\"nro\":\"switch/PlayShelf/PlayShelf.nro\""));
    CHECK(has(t, "\"id\":\"pin_missing\",\"level\":\"todo\",\"title\":\"No PIN yet\""));
    CHECK(has(t, "\"route\":\"security\"") && has(t, "\"level\":\"info\"") && has(t, "\"lang\":\"en\""));
    CHECK(has(t, "\"id\":\"odd\"") && has(t, "\"route\":\"\""));   // a tab name that is not a route: the home
    CHECK(has(t, "\"updated\":1000"));

    // The same entries later: nothing to write.
    CHECK(text(entries, "en", "/switch/PlayShelf/PlayShelf.nro", t, 2000).empty());
    // One changed: written, with its new time; the others keep theirs.
    std::vector<Entry> moved = entries;
    moved[1].detail = "13 h played";
    const std::string t2 = text(moved, "en", "/switch/PlayShelf/PlayShelf.nro", t, 2000);
    CHECK(has(t2, "13 h played") && has(t2, "\"updated\":2000") && has(t2, "\"updated\":1000"));
    // Another language: every entry written again.
    const std::string t3 = text(entries, "fr", "/switch/PlayShelf/PlayShelf.nro", t, 3000);
    CHECK(has(t3, "\"lang\":\"fr\"") && !has(t3, "\"updated\":1000"));
    // None left: an empty list.
    CHECK(has(text({}, "en", "/switch/PlayShelf/PlayShelf.nro", t, 4000), "\"notices\":[]"));
}

static void test_previous()
{
    // Started from nxlink (no path): the .nro written before is kept, with
    // the members PlayShelf does not know.
    const std::string before =
        "{\"schema\":1,\"module\":\"playshelf\",\"nro\":\"switch/PS/PS.nro\",\"future\":true,"
        "\"notices\":[{\"id\":\"week\",\"level\":\"info\",\"title\":\"Your week\",\"detail\":\"12 h played\","
        "\"lang\":\"en\",\"updated\":5,\"expires\":0,\"route\":\"activity\",\"icon\":\"clock\"}]}";
    const std::string t = text({ { "week", false, "Your week", "12 h played", "activity" },
                                 { "update", true, "Update", "1.5.0", "about" } },
                               "en", "", before, 900);
    CHECK(has(t, "\"nro\":\"switch/PS/PS.nro\"") && has(t, ",\"future\":true}"));
    CHECK(has(t, "\"updated\":5,\"expires\":0,\"route\":\"activity\",\"icon\":\"clock\"}"));
    CHECK(has(t, "\"updated\":900"));
    // A newer version's file: never written over.
    const std::string newer = "{\"schema\":2,\"notices\":[]}";
    CHECK(text({ { "week", false, "x", "", "" } }, "en", "", newer, 1).empty());
    // Not a file (damaged): written anew.
    CHECK(has(text({ { "week", false, "x", "", "" } }, "en", "", "{oops", 1), "\"id\":\"week\""));
}

static void test_bounds()
{
    // At most BL_NOTICES_MAX entries, the first ones; a twice-seen id once;
    // a title cut before a character that does not fit whole.
    std::vector<Entry> many;
    for (int i = 0; i < 20; i++) many.push_back({ "n" + std::to_string(i), false, "x", "", "" });
    many.push_back({ "n0", true, "again", "", "" });
    const std::string t = text(many, "en", "/switch/x.nro", "", 1);
    CHECK(has(t, "\"id\":\"n15\"") && !has(t, "\"id\":\"n16\"") && !has(t, "again"));
    std::string title(95, 'a');
    title += "é";   // 2 bytes: 97 with the '\0' fits not
    const std::string t2 = text({ { "long", false, title, "", "" } }, "en", "/switch/x.nro", "", 1);
    CHECK(has(t2, "\"title\":\"" + std::string(95, 'a') + "\""));
}

static void put(const std::string& path, const std::string& text)
{
    CHECK(std::system(("mkdir -p '" + path.substr(0, path.rfind('/')) + "'").c_str()) == 0);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!CHECK(f)) return;
    std::fputs(text.c_str(), f);
    std::fclose(f);
}

static void test_others()
{
    TempDir sd("shared_bell");
    const std::string root = sd.path();
    CHECK(shared_bell_logic::others(root, "en", 100).empty());   // no folder yet
    // Lantern: two shown (one to do with its progress), one expired, one in
    // French; its name from the registry. Atelier: its .nro gone. PlayShelf's
    // own file: not another's. A file that is not one: skipped.
    put(root + "/switch/Lantern/Lantern.nro", "NRO0");
    put(root + "/config/boreale/apps/lantern.conf", "schema=1\nname=Lantern\nversion=0.3.0\n");
    put(root + "/config/boreale/notices/lantern.json",
        "{\"schema\":1,\"module\":\"lantern\",\"nro\":\"switch/Lantern/Lantern.nro\",\"notices\":["
        "{\"id\":\"dl\",\"level\":\"progress\",\"title\":\"Downloading\",\"detail\":\"Star Kart\",\"lang\":\"en\",\"progress\":40,\"route\":\"downloads\"},"
        "{\"id\":\"fail\",\"level\":\"error\",\"title\":\"Install failed\",\"lang\":\"en\",\"route\":\"downloads/queue\"},"
        "{\"id\":\"old\",\"level\":\"todo\",\"title\":\"Old\",\"lang\":\"en\",\"expires\":50},"
        "{\"id\":\"fr\",\"level\":\"todo\",\"title\":\"En français\",\"lang\":\"fr\"}]}");
    put(root + "/config/boreale/notices/atelier.json",
        "{\"schema\":1,\"module\":\"atelier\",\"nro\":\"switch/Atelier/Atelier.nro\",\"notices\":[{\"id\":\"a\",\"level\":\"todo\",\"title\":\"x\"}]}");
    put(root + "/config/boreale/notices/playshelf.json",
        "{\"schema\":1,\"module\":\"playshelf\",\"nro\":\"switch/Lantern/Lantern.nro\",\"notices\":[{\"id\":\"a\",\"level\":\"todo\",\"title\":\"x\"}]}");
    put(root + "/config/boreale/notices/broken.json", "{oops");
    // Kite: no registry file, nothing to do but information.
    put(root + "/switch/kite.nro", "NRO0");
    put(root + "/config/boreale/notices/kite.json",
        "{\"schema\":1,\"nro\":\"/switch/kite.nro\",\"notices\":[{\"id\":\"i\",\"level\":\"info\",\"title\":\"Ready\"}]}");

    // Hawk: started from nxlink, no .nro known: kept, with nothing to launch.
    put(root + "/config/boreale/notices/hawk.json",
        "{\"schema\":1,\"nro\":\"\",\"notices\":[{\"id\":\"h\",\"level\":\"todo\",\"title\":\"Hi\"}]}");
    auto all = shared_bell_logic::others(root, "en", 100);
    if (!CHECK(all.size() == 3)) return;
    CHECK(all[1].name == "hawk" && all[1].nro.empty() && all[1].notices.size() == 1);
    all.erase(all.begin() + 1);
    CHECK(all[0].name == "Lantern" && all[0].module == "lantern" && all[0].nro == "/switch/Lantern/Lantern.nro");
    CHECK(all[1].name == "kite" && all[1].nro == "/switch/kite.nro" && all[1].notices.size() == 1 && !all[1].notices[0].attention);
    if (!CHECK(all[0].notices.size() == 2)) return;
    CHECK(all[0].notices[0].id == "dl" && !all[0].notices[0].attention && all[0].notices[0].detail == "Star Kart · 40\u00a0%");
    CHECK(all[0].notices[0].route == "downloads");
    CHECK(all[0].notices[1].id == "fail" && all[0].notices[1].attention && all[0].notices[1].route == "downloads/queue");
    // In French: its French notice instead.
    const auto fr = shared_bell_logic::others(root, "fr", 100);
    CHECK(fr.size() == 3 && fr[0].notices.size() == 1 && fr[0].notices[0].id == "fr");
}

static void test_app()
{
    CHECK(shared_bell_logic::app_file("/") == "/config/boreale/apps/playshelf.conf");
    const std::string t = shared_bell_logic::app_text("1.4.0", "/switch/PlayShelf/PlayShelf.nro", "");
    CHECK(has(t, "name=PlayShelf\n") && has(t, "version=1.4.0\n") && has(t, "nro=switch/PlayShelf/PlayShelf.nro\n"));
    CHECK(has(t, "repo=JigSawFr/PlayShelf\n"));
    CHECK(shared_bell_logic::app_text("1.4.0", "/switch/PlayShelf/PlayShelf.nro", t).empty());   // unchanged: not written
    // A new version from nxlink (no path): the .nro and a newer version's key kept.
    const std::string t2 = shared_bell_logic::app_text("1.5.0", "", t + "future_key=1\n");
    CHECK(has(t2, "version=1.5.0\n") && has(t2, "nro=switch/PlayShelf/PlayShelf.nro\n") && has(t2, "future_key=1\n"));
    CHECK(shared_bell_logic::app_text("1.5.0", "", "schema=2\nname=PlayShelf\n").empty());   // a newer file: left alone
}

static void test_routes()
{
    // Each entry to the tab where it is dealt with: a tab of the main screen.
    CHECK(shared_bell_logic::route_of("updates") == "library" && shared_bell_logic::route_of("dlcs") == "library");
    CHECK(shared_bell_logic::route_of("wish.0100000000010000") == "catalogue");
    CHECK(shared_bell_logic::route_of("out.0100000000010000.20261102") == "catalogue");
    CHECK(shared_bell_logic::route_of("atlas_new.0100000000010000.20261102") == "catalogue");
    CHECK(shared_bell_logic::route_of("downloads_failed") == "downloads" && shared_bell_logic::route_of("kit.update") == "about");
    CHECK(shared_bell_logic::route_of("storage") == "home");
}

int main()
{
    CHECK(shared_bell_logic::file("/") == "/config/boreale/notices/playshelf.json");
    CHECK(shared_bell_logic::file("./sd") == "./sd/config/boreale/notices/playshelf.json");
    test_written();
    test_previous();
    test_bounds();
    test_others();
    test_app();
    test_routes();
    return CHECK_DONE("shared_bell_logic: written, kept, bounded, the others read, the registry, the routes assertions passed");
}
