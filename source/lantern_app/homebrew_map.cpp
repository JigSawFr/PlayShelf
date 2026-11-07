// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/homebrew_map.hpp"

#include "lantern_app/session.hpp"
#include "library/catalog.hpp"

namespace lantern_app
{

namespace
{
constexpr uint32_t PER_PAGE = 100;
constexpr int      MAX_PAGES = 20;

std::string https_or_empty(const std::string& origin, std::string_view url)
{
    const std::string abs = library::resolve(origin, std::string(url));
    return abs.rfind("https://", 0) == 0 ? abs : std::string();
}
}   // namespace

void add_homebrew(HomebrewList& list, std::span<const lantern::Homebrew> entries, const std::string& origin,
                  const std::string& arch)
{
    for (const lantern::Homebrew& e : entries) {
        if (list.items.size() >= MAX_HOMEBREW) {
            list.truncated = true;
            return;
        }
        const auto package = lantern::pick_package(e, arch);
        const std::string url = package ? https_or_empty(origin, (*package)->file) : std::string();
        if (url.empty() || e.id.empty()) {
            list.skipped++;
            continue;
        }
        HomebrewItem item;
        item.entry       = e;
        item.url         = url;
        item.size        = (*package)->size;
        item.name        = library::display_text(std::string(e.name), 128);
        item.author      = library::display_text(std::string(e.author), 128);
        item.summary     = library::display_text(std::string(e.summary), 256);
        item.description = library::display_text(std::string(e.description), 2048);
        item.version     = library::display_text(std::string(e.version), 32);
        if (e.icon) item.icon = https_or_empty(origin, e.icon->url);
        if (item.name.empty()) item.name = std::string(e.id);
        list.items.push_back(std::move(item));
    }
}

lantern::HomebrewUpdatesRequest updates_request(std::span<const Held> held)
{
    lantern::HomebrewUpdatesRequest r;
    for (const Held& h : held) {
        lantern::InstalledHomebrew i;
        i.id    = h.id;
        i.build = h.build;
        r.installed.push_back(std::move(i));
    }
    return r;
}

std::string fetch_homebrew(lantern::Client& client, const std::string& platform, const std::string& origin,
                           const std::string& arch, HomebrewList& out)
{
    lantern::HomebrewQuery query;
    query.platform  = platform;
    query.page_size = PER_PAGE;
    std::string cursor;
    for (int page = 0; page < MAX_PAGES; page++) {
        query.cursor = cursor;
        const auto r = client.homebrew(query);
        if (!r) return describe(r.error());
        if (!*r) return "the server lists its homebrew in a newer version than this one reads";
        add_homebrew(out, (*r)->items, origin, arch);
        cursor.assign((*r)->next);
        if (cursor.empty() || out.truncated) return {};
    }
    out.truncated = true;
    return {};
}

std::string fetch_homebrew_updates(lantern::Client& client, std::span<const Held> held, const std::string& platform,
                                   const std::string& origin, const std::string& arch, HomebrewList& out)
{
    if (held.empty()) return {};
    const lantern::HomebrewUpdatesRequest request = updates_request(held);
    std::string cursor;
    for (int page = 0; page < MAX_PAGES; page++) {
        const auto r = client.homebrew_updates(request, cursor, platform);
        if (!r) return describe(r.error());
        if (!*r) return "the server lists its homebrew in a newer version than this one reads";
        add_homebrew(out, (*r)->items, origin, arch);
        cursor.assign((*r)->next);
        if (cursor.empty() || out.truncated) return {};
    }
    out.truncated = true;
    return {};
}

}   // namespace lantern_app
