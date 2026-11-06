// source/lantern/catalog_map.cpp: Lantern apps and sections as the neutral
// catalogue: kinds, versions, addresses made absolute and https only, an app
// with a bad title id left out, sections over the items by app id.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "lantern/catalog_map.hpp"

using namespace lantern_app;

namespace
{
lantern::App app(const char* id, const char* title, const char* type, uint64_t version, const char* file)
{
    lantern::App a;
    a.id = id;
    a.app_id = title;
    a.type = type;
    a.version_code = version;
    a.name = "Tide Runner";
    a.size = 1234;
    a.file = file;
    return a;
}
}   // namespace

int main()
{
    library::Catalog catalog;
    AppIndex index;
    const std::vector<lantern::App> apps = {
        app("a1", "0100ABCD00010000", "base", 0, "/files/a1.nsp"),
        app("a2", "0100ABCD00010800", "update", 65536, "https://cdn.example.org/a2.nsp"),
        app("a3", "0100ABCD00011001", "dlc", 0, "/files/a3.nsp"),
        app("a4", "not-a-title-id", "base", 0, "/files/a4.nsp"),
        app("a5", "0100ABCD00020000", "base", 0, "http://shelf.example.org/a5.nsp"),
        app("a6", "0100ABCD00030000", "weird", 0, "/files/a6.nsp"),
    };
    add_apps(catalog, index, apps, "https://shelf.example.org");

    CHECK(catalog.items.size() == 4 && catalog.skipped == 2);
    CHECK(catalog.items[0].url == "https://shelf.example.org/files/a1.nsp");
    CHECK(catalog.items[0].title_id == 0x0100ABCD00010000ULL && catalog.items[0].kind == library::Kind::Base);
    CHECK(catalog.items[1].url == "https://cdn.example.org/a2.nsp" && catalog.items[1].version == 65536
          && catalog.items[1].has_version && catalog.items[1].kind == library::Kind::Update);
    CHECK(catalog.items[2].kind == library::Kind::Dlc);
    CHECK(catalog.items[3].kind == library::Kind::Base);   // an unknown type: told by its title id
    CHECK(index.at("a2") == 1 && !index.contains("a4"));

    lantern::Section s;
    s.id = "new";
    s.title = "New games";
    s.apps = { "a2", "a1", "missing" };
    add_sections(catalog, index, std::span<const lantern::Section>(&s, 1));
    CHECK(catalog.sections.size() == 1 && catalog.sections[0].items == std::vector<size_t>({ 1, 0 }));
    CHECK(catalog.items[0].section == "new" && catalog.items[1].section == "new" && catalog.items[2].section.empty());

    CHECK_DONE("lantern catalogue: kinds, absolute https addresses, sections by app id passed");
}
