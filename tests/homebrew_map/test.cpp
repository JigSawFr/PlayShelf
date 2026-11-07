// source/lantern_app/homebrew_map.cpp: entries mapped for the tab: file made
// absolute and https only, a bad one counted and left out, texts made safe.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "lantern_app/homebrew_map.hpp"

using namespace lantern_app;

namespace
{
lantern::Homebrew entry(const char* id, const char* name, const char* file)
{
    lantern::Homebrew h;
    h.id = id;
    h.name = name;
    h.platform = "switch";
    h.version = "1.2.0";
    h.package.format = "nro";
    h.package.file = file;
    h.package.size = 4096;
    return h;
}
}   // namespace

int main()
{
    HomebrewList list;
    const std::vector<lantern::Homebrew> entries = {
        entry("org.example.tide", "Tide Tool", "/files/tide.nro"),
        entry("org.example.cdn", "Cdn Tool", "https://cdn.example.org/cdn.nro"),
        entry("org.example.plain", "Plain", "http://shelf.example.org/plain.nro"),
        entry("org.example.blank", "", "/files/blank.nro"),
    };
    add_homebrew(list, entries, "https://shelf.example.org", "");
    CHECK(list.items.size() == 3 && list.skipped == 1);
    CHECK(list.items[0].url == "https://shelf.example.org/files/tide.nro");
    CHECK(list.items[1].url == "https://cdn.example.org/cdn.nro");
    CHECK(list.items[0].size == 4096 && list.items[0].name == "Tide Tool");
    CHECK(list.items[2].name == "org.example.blank");   // no name: the id

    // A port is listed when its data lies where § 8.6 allows, and counted as
    // skipped when it does not.
    lantern::Homebrew good = entry("org.example.frost", "Frost", "/files/frost.nro");
    good.kind = "port";
    good.port.emplace();
    good.port->data_root = "/switch/frost/";
    lantern::Homebrew bad = good;
    bad.id = "org.example.bad";
    bad.port->data_root = "/etc/";
    HomebrewList ports;
    add_homebrew(ports, std::vector<lantern::Homebrew>{good, bad}, "https://shelf.example.org", "");
    CHECK(ports.items.size() == 1 && ports.skipped == 1);

    CHECK_DONE("homebrew map: files absolute and https, bad entries counted");
}
