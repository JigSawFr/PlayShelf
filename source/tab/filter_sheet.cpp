// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/filter_sheet.hpp"

#include <algorithm>
#include <memory>
#include <set>

#include <aurore/cadran/ui/live_prefs.hpp>

#include "action/nlib_flow.hpp"
#include "library/titles.hpp"
#include "titledb/nlib.hpp"
#include "ui/ui.hpp"

using namespace aurore::literals;

namespace filter_sheet
{
namespace
{
using State = library::Filter::State;

struct Sheet
{
    library::Filter*      filter;
    std::vector<uint64_t> games;
    States                states;
    Order                 order;
    std::function<void()> changed;
};

const char* state_key(State s)
{
    switch (s) {
        case State::Missing: return "playshelf/filters/state_missing";
        case State::Installed: return "playshelf/filters/state_installed";
        case State::ToUpdate: return "playshelf/filters/state_to_update";
        case State::DlcsMissing: return "playshelf/filters/state_dlcs_missing";
        case State::NeverPlayed: return "playshelf/filters/state_never_played";
        default: return "playshelf/filters/any";
    }
}

std::string players_text(int players)
{
    return players == 1 ? "playshelf/filters/players_one"_i18n
         : players >= 2 ? "playshelf/filters/players_many"_i18n
                        : "playshelf/filters/any"_i18n;
}

// The language PlayShelf speaks, as Nlib API names it.
std::string own_language()
{
    return titledb::nlib_language(live_prefs::locale());
}

void show(std::shared_ptr<Sheet> sheet);

// A choice made: applied, told, the sheet open again.
std::function<void()> then(const std::shared_ptr<Sheet>& sheet, std::function<void(library::Filter&)> change)
{
    return [sheet, change]() {
        change(*sheet->filter);
        if (sheet->changed) sheet->changed();
        show(sheet);
    };
}

// A picker among `values` (their words `labels`, "" first: any), for the
// field `field` of the filter.
void pick_value(const std::shared_ptr<Sheet>& sheet, const std::string& title, std::vector<std::string> values,
                std::vector<std::string> labels, std::string library::Filter::*field)
{
    values.insert(values.begin(), "");
    labels.insert(labels.begin(), "playshelf/filters/any"_i18n);
    const auto at = std::find(values.begin(), values.end(), sheet->filter->*field);
    ui::pick(title, labels, at == values.end() ? 0 : (int)(at - values.begin()), [sheet, values, field](int i) {
        then(sheet, [&](library::Filter& f) { f.*field = values[(size_t)i]; })();
    });
}

void show(std::shared_ptr<Sheet> sheet)
{
    using Choice = ui::Choice;
    library::Filter&    f = *sheet->filter;
    std::vector<Choice> lines;

    // The genres and publishers the games' facts know.
    std::set<std::string> genres, publishers;
    for (uint64_t id : sheet->games) {
        const library::Facts facts = nlib_flow::facts(id);
        for (const std::string& g : facts.categories)
            if (g != "Other") genres.insert(g);
        if (!facts.publisher.empty()) publishers.insert(facts.publisher);
    }

    if (!sheet->order.labels.empty()) {
        const Order& o = sheet->order;
        lines.emplace_back("playshelf/library/sort"_i18n, [sheet]() {
            const Order& order = sheet->order;
            ui::pick("playshelf/library/sort"_i18n, order.labels, order.chosen, [sheet](int i) {
                sheet->order.chosen = i;
                if (sheet->order.pick) sheet->order.pick(i);
                show(sheet);
            });
        }, "list", o.labels[(size_t)std::min<int>(o.chosen, (int)o.labels.size() - 1)]);
    }
    lines.emplace_back("playshelf/filters/genre"_i18n, [sheet, genres]() {
        std::vector<std::string> values(genres.begin(), genres.end()), labels;
        // In the order of their words.
        std::sort(values.begin(), values.end(), [](const std::string& a, const std::string& b) {
            return library::fold(nlib_flow::genre_name(a)) < library::fold(nlib_flow::genre_name(b));
        });
        for (const std::string& g : values) labels.push_back(nlib_flow::genre_name(g));
        pick_value(sheet, "playshelf/filters/genre"_i18n, values, labels, &library::Filter::genre);
    }, "folder", f.genre.empty() ? "playshelf/filters/any"_i18n : nlib_flow::genre_name(f.genre));
    lines.emplace_back("playshelf/filters/publisher"_i18n, [sheet, publishers]() {
        std::vector<std::string> values(publishers.begin(), publishers.end());
        pick_value(sheet, "playshelf/filters/publisher"_i18n, values, values, &library::Filter::publisher);
    }, "server", f.publisher.empty() ? "playshelf/filters/any"_i18n : f.publisher);
    lines.emplace_back("playshelf/filters/players"_i18n, [sheet]() {
        const std::vector<std::string> labels = { players_text(0), players_text(1), players_text(2) };
        ui::pick("playshelf/filters/players"_i18n, labels, std::min(sheet->filter->players, 2), [sheet](int i) {
            then(sheet, [i](library::Filter& x) { x.players = i; })();
        });
    }, "gamepad", players_text(f.players));
    const std::string own = own_language();
    lines.emplace_back("playshelf/game/language_chip"_i18n, then(sheet, [own](library::Filter& x) {
        x.language = x.language.empty() ? own : "";
    }), "globe").switched(!f.language.empty());

    // What the console has of them.
    std::vector<State> states = { State::Any };
    if (sheet->states == States::Catalogue) states.insert(states.end(), { State::Missing, State::Installed });
    states.insert(states.end(), { State::ToUpdate, State::DlcsMissing });
    if (sheet->states == States::Library) states.push_back(State::NeverPlayed);
    lines.emplace_back("playshelf/filters/state"_i18n, [sheet, states]() {
        std::vector<std::string> labels;
        for (State s : states) labels.push_back(aurore::getStr(state_key(s)));
        const auto at = std::find(states.begin(), states.end(), sheet->filter->state);
        ui::pick("playshelf/filters/state"_i18n, labels, at == states.end() ? 0 : (int)(at - states.begin()),
                 [sheet, states](int i) { then(sheet, [&](library::Filter& x) { x.state = states[(size_t)i]; })(); });
    }, "download", aurore::getStr(state_key(f.state)));
    lines.emplace_back("playshelf/filters/hide_demos"_i18n, then(sheet, [](library::Filter& x) {
        x.hide_demos = !x.hide_demos;
    }), "eye").switched(f.hide_demos);
    if (f.active())
        lines.emplace_back("playshelf/filters/clear"_i18n, then(sheet, [](library::Filter& x) { x = library::Filter(); }),
                           "trash", "", "", Choice::Tone::DANGER);
    ui::pick_actions("playshelf/filters/title"_i18n, lines, "playshelf/filters/note"_i18n);
}
}   // namespace

void open(library::Filter* filter, std::vector<uint64_t> games, States states, Order order, std::function<void()> changed)
{
    if (!filter) return;
    show(std::make_shared<Sheet>(Sheet{ filter, std::move(games), states, std::move(order), std::move(changed) }));
}

std::string summary(const library::Filter& f)
{
    std::vector<std::string> parts;
    if (!f.genre.empty()) parts.push_back(nlib_flow::genre_name(f.genre));
    if (!f.publisher.empty()) parts.push_back(f.publisher);
    if (f.players) parts.push_back(players_text(f.players));
    if (!f.language.empty()) parts.push_back("playshelf/game/language_chip"_i18n);
    if (f.state != State::Any) parts.push_back(aurore::getStr(state_key(f.state)));
    if (f.hide_demos) parts.push_back("playshelf/filters/no_demos"_i18n);
    std::string out;
    for (size_t i = 0; i < parts.size() && i < 2; i++) out += (i ? " · " : "") + parts[i];
    if (parts.size() > 2) out += " · +" + std::to_string(parts.size() - 2);
    return out;
}

}   // namespace filter_sheet
