// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/game_page.hpp"

#include <algorithm>

#include "action/library_icons.hpp"
#include "action/insights.hpp"
#include "action/nlib_flow.hpp"
#include "action/popular_flow.hpp"
#include "action/services_flow.hpp"
#include "activity/game_activity.hpp"
#include "library/servers.hpp"

namespace game_page
{

void open(const std::shared_ptr<const library_flow::Loaded>& data, size_t index)
{
    if (!data || index >= data->titles.size()) return;
    const std::string password = library::password_of(data->server);
    const library::Title& t = data->titles[index];
    GameActivity::Page    page;
    page.name = t.name;
    page.title_id = t.id;
    // An AeroFoil server's own for this game: its cheats, its save backups.
    page.lines = library_flow::game_lines(data, index, services_flow::game_choices(data, t.id));
    auto icon_of = [data, password](size_t i) -> GameActivity::IconAsk {
        const std::string url = data->titles[i].icon_url;
        const uint64_t    game_id = data->titles[i].id;
        return [data, password, url, game_id](library_icons::Done shown) {
            if (!url.empty()) library_icons::get(data, password, url, std::move(shown), game_id);
            else library_icons::local(game_id, std::move(shown));
        };
    };
    page.icon = icon_of(index);
    auto others = [data, password, index, icon_of](const std::function<bool(const library::Facts&)>& like) {
        std::vector<GameActivity::Other> out;
        for (size_t i = 0; i < data->titles.size(); i++) {
            if (i == index || !data->titles[i].id || !like(nlib_flow::facts(data->titles[i].id))) continue;
            out.push_back({ data->titles[i].name, icon_of(i), [data, password, i]() { open(data, i); } });
        }
        return out;
    };
    page.by_publisher = [others](const std::string& publisher) {
        return others([&](const library::Facts& f) { return f.publisher == publisher; });
    };
    page.by_genre = [others](const std::string& genre) {
        return others([&](const library::Facts& f) {
            return std::ranges::contains(f.categories, genre);
        });
    };
    // The games most like it (PlayInsights), on any server read, the most alike first.
    page.similar = [id = t.id]() {
        std::vector<GameActivity::Other> out;
        const library::SimilarBook::Row* row = popular_flow::similar()->find(id);
        if (!row) return out;
        for (const auto& [other, score] : *row) {
            const insights::Pick p = insights::found_of(other);
            if (!p.data) continue;
            const auto        found = p.data;
            const size_t      i = p.index;
            const std::string url = p.title().icon_url;
            const std::string pass = library::password_of(found->server);
            out.push_back({ p.title().name,
                            [found, pass, url, other](library_icons::Done shown) {
                                if (!url.empty()) library_icons::get(found, pass, url, std::move(shown), other);
                                else library_icons::local(other, std::move(shown));
                            },
                            [found, i]() { open(found, i); } });
        }
        return out;
    };
    GameActivity::show(std::move(page));
}

}   // namespace game_page
