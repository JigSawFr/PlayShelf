// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern/catalog_map.hpp"

#include <map>

namespace lantern_app
{
namespace
{
bool hex16(const std::string& s, uint64_t* out)
{
    if (s.size() != 16) return false;
    uint64_t v = 0;
    for (const char c : s) {
        const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return false;
        v = v << 4 | (uint64_t)d;
    }
    *out = v;
    return true;
}

library::Kind kind_of(const std::string& type, uint64_t title_id)
{
    if (type == "base") return library::Kind::Base;
    if (type == "update") return library::Kind::Update;
    if (type == "dlc") return library::Kind::Dlc;
    return library::kind_of(title_id);
}
}   // namespace

void add_apps(library::Catalog& catalog, AppIndex& index, std::span<const lantern::App> apps, const std::string& origin)
{
    for (const lantern::App& app : apps) {
        if (catalog.items.size() >= library::MAX_ITEMS) {
            catalog.truncated = true;
            return;
        }
        library::Item item;
        const std::string url = library::resolve(origin, std::string(app.file));
        if (url.empty() || !hex16(std::string(app.title_id), &item.title_id)) {
            catalog.skipped++;
            continue;
        }
        item.url         = url;
        item.name        = library::display_text(std::string(app.name), 128);
        item.size        = app.size;
        item.version     = app.version_code > UINT32_MAX ? UINT32_MAX : (uint32_t)app.version_code;
        item.has_version = true;
        item.kind        = kind_of(std::string(app.type), item.title_id);
        index[std::string(app.id)] = catalog.items.size();
        catalog.items.push_back(std::move(item));
    }
}

void add_sections(library::Catalog& catalog, const AppIndex& index, std::span<const lantern::Section> sections)
{
    for (const lantern::Section& s : sections) {
        library::Section out;
        out.id    = library::display_text(std::string(s.id), 64);
        out.title = library::display_text(std::string(s.title), 128);
        for (const auto& id : s.apps) {
            const auto it = index.find(std::string(id));
            if (it == index.end()) continue;
            out.items.push_back(it->second);
            if (catalog.items[it->second].section.empty()) catalog.items[it->second].section = out.id;
        }
        catalog.sections.push_back(std::move(out));
    }
}

}   // namespace lantern_app
