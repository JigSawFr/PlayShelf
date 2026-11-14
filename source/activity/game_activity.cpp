// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/game_activity.hpp"

#include <algorithm>
#include <ctime>
#include <format>
#include <utility>

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/ui/cadran_list.hpp>
#include <aurore/cadran/ui/live_prefs.hpp>
#include <aurore/cadran/view/cadran_icon.hpp>

#include "action/installed_flow.hpp"
#include "action/library_icons.hpp"
#include "action/nlib_flow.hpp"
#include "action/popular_flow.hpp"
#include "action/wishlist.hpp"
#include "library/catalog.hpp"
#include "library/servers.hpp"
#include "tab/catalogue_tab.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/title_id.hpp"
#include "view/cover_placeholder.hpp"
#include "view/game_parts.hpp"
#include "view/game_tile.hpp"

using namespace aurore::literals;
using namespace game_parts;

namespace
{
namespace kit = cadran::kit;

constexpr float ICON = 112;
constexpr int      SHOTS_PER_ROW = 3;
constexpr size_t   MAX_OTHERS = 12;        // games in a row of more like it
constexpr uint64_t PLAYERS_TRUSTED = 20;   // IGDB ratings below which the players' rating is not shown

// How long the console played it ("12 h played"), "" when never.
std::string played_text(uint64_t title_id)
{
    if (const auto games = installed_flow::games())
        for (const installed_flow::Game& g : *games)
            if (g.game.id == title_id && g.play_s) return installed_flow::play_text(g.play_s);
    return "";
}

// The one score worth a chip: the critics' when there is one, else the
// players' (IGDB, from enough of them), else Steam's share of positive
// reviews. 0 when none.
int score_of(uint64_t title_id)
{
    if (const int critics = popular_flow::of(title_id).critics) return critics;
    const library::GameSignals* sig = popular_flow::signals()->find(title_id);
    if (!sig) return 0;
    if (sig->igdb.count >= PLAYERS_TRUSTED) return (int)sig->igdb.rating;
    const uint64_t reviews = sig->steam.positive + sig->steam.negative;
    return reviews ? (int)(sig->steam.positive * 100 / reviews) : 0;
}

enum Tab : size_t { OVERVIEW, UPDATES, DLCS, DETAILS };

}   // namespace

void GameActivity::show(Page page)
{
    aurore::Application::pushActivity(new GameActivity(std::move(page)));
}

GameActivity::~GameActivity()
{
    *this->alive = false;
}

void GameActivity::onContentAvailable()
{
    if (auto* applet = dynamic_cast<aurore::AppletFrame*>(this->getContentView())) applet->setTitle(this->page.name);
    this->banner = new Banner();
    hero->addView(this->banner, 0);
    cadran::List::section(screens_header);
    cadran::List::section(about_header);
    this->add_head();
    this->add_tabs();
    this->fill_details_tab();
    // The focus on its first line, as the sheet had it (A, A installs
    // everything); the tabs above it.
    if (!lines_box->getChildren().empty()) {
        aurore::View* first = lines_box->getChildren()[0]->getDefaultFocus();
        ui::Weak    weak(this->alive);
        if (first)
            aurore::sync([weak, first]() {
                const ui::Alive held = weak.lock();
                if (held && *held) aurore::Application::giveFocus(first);
            });
    }

    // What Nlib API knows of it: asked once the page is up.
    if (this->page.title_id) {
        ui::Weak weak(this->alive);
        nlib_flow::details(this->page.title_id, [this, weak](std::shared_ptr<const titledb::Details> d) {
            const ui::Alive held = weak.lock();
            if (held && *held && d) this->show_details(d);
        });
    }
}

// Its icon, its name, its facts; what Nlib API knows of it under them.
void GameActivity::add_head()
{
    auto* cover = new aurore::Box(aurore::Axis::ROW);
    cover->setWidth(ICON);
    cover->setHeight(ICON);
    cover->setShrink(0);
    cover->setCornerRadius(12);
    cover->setThemeColor("backgroundColor", "cadran/well");
    cover->setAlignItems(aurore::AlignItems::CENTER);
    cover->setJustifyContent(aurore::JustifyContent::CENTER);
    cover->setMarginRight(24);
    cover->setShadowType(aurore::ShadowType::GENERIC);
    cover->setShadowVisibility(true);
    auto* placeholder = new CoverPlaceholder(ICON, 48, 12);
    cover->addView(placeholder);
    this->icon = new aurore::Image();
    this->icon->setWidth(ICON);
    this->icon->setHeight(ICON);
    this->icon->setCornerRadius(12);
    this->icon->setScalingType(aurore::ImageScalingType::FIT);
    this->icon->setVisibility(aurore::Visibility::GONE);
    cover->addView(this->icon);
    head->addView(cover);

    auto* text = new aurore::Box(aurore::Axis::COLUMN);
    text->setGrow(1);
    text->setShrink(1);
    this->title = kit::display(this->page.name, 32);
    text->addView(this->title);
    this->meta = kit::label("", 17, "cadran/text2");
    this->meta->setMarginTop(6);
    this->meta->setVisibility(aurore::Visibility::GONE);
    text->addView(this->meta);
    std::string    state = this->page.lines.facts;
    if (const std::string played = played_text(this->page.title_id); !played.empty())
        state += (state.empty() ? "" : " · ") + played;
    aurore::Label* facts_line = kit::label(state, 16, "cadran/text3");
    facts_line->setMarginTop(4);
    text->addView(facts_line);
    if (!this->page.note.empty()) {
        aurore::Label* note = kit::label(this->page.note, 16, "cadran/amber");
        note->setMarginTop(6);
        text->addView(note);
    }
    this->chips = new aurore::Box(aurore::Axis::ROW);
    this->chips->setMarginTop(10);
    this->chips->setVisibility(aurore::Visibility::GONE);
    text->addView(this->chips);
    head->addView(text);

    if (this->page.icon) {
        ui::Weak weak(this->alive);
        this->page.icon([this, weak, placeholder](const library_icons::Picture& p) {
            const ui::Alive held = weak.lock();
            if (!held || !*held || !library_icons::show(this->icon, p)) return;
            this->icon->setVisibility(aurore::Visibility::VISIBLE);
            placeholder->setVisibility(aurore::Visibility::GONE);
        });
    }
    this->show_chips();
}

// The tabs, each a soft chip that A (or L and R) shows; Updates and
// Add-ons only when it has some. The panes' lines.
void GameActivity::add_tabs()
{
    const library_flow::GameLines& l = this->page.lines;
    const std::pair<Tab, std::string> TABS[] = {
        { OVERVIEW, "playshelf/game/tab_overview"_i18n },
        { UPDATES, aurore::getStr("playshelf/game/tab_updates", l.update_count) },
        { DLCS, aurore::getStr("playshelf/game/tab_dlcs", l.dlc_count) },
        { DETAILS, "playshelf/game/tab_details"_i18n },
    };
    this->panes = { overview, updates, dlcs, facts };
    for (const auto& [tab, label] : TABS) {
        auto* box = new aurore::Box(aurore::Axis::ROW);
        box->setPadding(10, 22, 10, 22);
        box->setMarginRight(10);
        box->setCornerRadius(22);
        box->setHighlightCornerRadius(24);
        box->setFocusable(true);
        box->addView(kit::label(label, 18, "cadran/text2"));
        const size_t index = tab;
        box->registerClickAction([this, index](aurore::View*) {
            this->select(index, true);
            return true;
        });
        const bool shown = tab == OVERVIEW || tab == DETAILS || (tab == UPDATES && l.update_count) || (tab == DLCS && l.dlc_count);
        box->setVisibility(shown ? aurore::Visibility::VISIBLE : aurore::Visibility::GONE);
        tab_bar->addView(box);
        this->tabs.push_back(box);
    }
    // L and R: the tab before and after, of those shown.
    auto step = [this](int by) {
        return [this, by](aurore::View*) {
            size_t at = this->current;
            for (size_t n = 0; n < this->tabs.size(); n++) {
                at = (at + this->tabs.size() + (size_t)by) % this->tabs.size();
                if (this->tabs[at]->getVisibility() == aurore::Visibility::VISIBLE) break;
            }
            this->select(at, true);
            return true;
        };
    };
    this->getContentView()->registerAction("playshelf/game/tab_previous"_i18n, aurore::BUTTON_LB, step(-1));
    this->getContentView()->registerAction("playshelf/game/tab_next"_i18n, aurore::BUTTON_RB, step(1));
    // X: on the wishlist (Home shows it, the bell says when a server has
    // it), or off it.
    if (this->page.title_id) {
        const uint64_t    game_id = this->page.title_id;
        const std::string name = this->page.name;
        auto hint = [game_id]() { return wishlist::has(game_id) ? "playshelf/wishlist/remove"_i18n : "playshelf/wishlist/add"_i18n; };
        this->getContentView()->registerAction(hint(), aurore::BUTTON_X, [this, game_id, name, hint](aurore::View*) {
            wishlist::toggle(game_id, name);
            this->getContentView()->updateActionHint(aurore::BUTTON_X, hint());
            aurore::Application::getGlobalHintsUpdateEvent()->fire();
            return true;
        });
    }

    // Overview: everything missing and the base game, then the caller's.
    std::vector<ui::Choice> main = l.main;
    main.insert(main.end(), l.more.begin(), l.more.end());
    this->add_lines(lines_box, main);
    // Where it is to be had: a chip a server (Details says what each has).
    const std::vector<library_flow::Source> sources = library_flow::sources(this->page.title_id);
    if (!sources.empty()) {
        auto* row = new aurore::Box(aurore::Axis::ROW);
        row->setAlignItems(aurore::AlignItems::CENTER);
        row->setMarginTop(14);
        aurore::Label* label = kit::label("playshelf/game/sources"_i18n, 15, "cadran/text3");
        label->setMarginRight(12);
        label->setMarginBottom(6);
        row->addView(label);
        for (const library_flow::Source& src : sources)
            row->addView(chip(src.server, "server", "cadran/text2", "cadran/surf2"));
        kit::wrap(row);
        lines_box->addView(row);
    }

    // Updates: the versions installed and newest, then the lines.
    if (l.update_count) {
        std::string line = l.installed ? aurore::getStr("playshelf/game/version_installed", l.installed_version) : "";
        if (l.latest_version)
            line += (line.empty() ? "" : " · ") + aurore::getStr("playshelf/game/version_latest", l.latest_version);
        const bool behind = l.installed && l.installed_version < l.latest_version;
        if (!l.installed) updates->addView(status_card("playshelf/game/not_installed"_i18n, line, "gamepad", "cadran/text2", "cadran/surf2"));
        else if (behind)
            updates->addView(status_card("playshelf/library/update_available"_i18n, line, "download", "cadran/amber", "cadran/amber_surf"));
        else updates->addView(status_card("playshelf/library/installed"_i18n, line, "check", "cadran/teal", "cadran/teal_soft"));
        this->add_lines(updates, l.updates);
    }
    // Add-ons: how many the console has, then the lines.
    if (l.dlc_count) {
        const std::string count = aurore::getStr("playshelf/game/dlcs_installed", l.dlcs_installed, l.dlc_count);
        if (l.dlcs_installed == l.dlc_count)
            dlcs->addView(status_card(count, l.dlcs_size ? library_flow::size_text(l.dlcs_size) : "", "check", "cadran/teal", "cadran/teal_soft"));
        else
            dlcs->addView(status_card(count, l.dlcs_size ? library_flow::size_text(l.dlcs_size) : "", "plus",
                                      l.installed ? "cadran/amber" : "cadran/text2", l.installed ? "cadran/amber_surf" : "cadran/surf2"));
        this->add_lines(dlcs, l.dlcs);
    }
    this->select(OVERVIEW, false);
}

void GameActivity::select(size_t tab, bool focus)
{
    if (tab >= this->tabs.size()) return;
    this->current = tab;
    for (size_t i = 0; i < this->tabs.size(); i++) {
        const bool on = i == tab;
        this->tabs[i]->setThemeColor("backgroundColor", on ? "cadran/teal_soft" : "cadran/surf2");
        if (auto* label = dynamic_cast<aurore::Label*>(this->tabs[i]->getChildren()[0]))
            label->setThemeColor("textColor", on ? "cadran/teal" : "cadran/text2");
        this->panes[i]->setVisibility(on ? aurore::Visibility::VISIBLE : aurore::Visibility::GONE);
    }
    if (focus) aurore::Application::giveFocus(this->tabs[tab]);
}

// The lines in a card, as the sheet had them: A runs one, the page gone
// first.
void GameActivity::add_lines(aurore::Box* pane, const std::vector<ui::Choice>& lines)
{
    if (lines.empty()) return;
    auto*       holder = new aurore::Box(aurore::Axis::COLUMN);
    holder->setAlignItems(aurore::AlignItems::STRETCH);
    pane->addView(holder);
    aurore::Box* card = cadran::List::rows_in(holder);
    for (const ui::Choice& c : lines) {
        auto* cell = new aurore::DetailCell();
        cell->setText(c.label);
        cell->setDetailText(c.detail);
        card->addView(cell);
        cadran::List::row(cell, c.icon);
        cell->detail->setSingleLine(true);
        cell->detail->setVisibility(c.detail.empty() ? aurore::Visibility::GONE : aurore::Visibility::VISIBLE);
        const char* tint = c.tone == ui::Choice::Tone::ON     ? "cadran/teal"
                         : c.tone == ui::Choice::Tone::WARN   ? "cadran/amber"
                         : c.tone == ui::Choice::Tone::DANGER ? "cadran/coral"
                                                              : nullptr;
        cell->detail->setThemeColor("textColor", tint ? tint : "cadran/text2");
        if (c.tone == ui::Choice::Tone::DANGER) cell->title->setThemeColor("textColor", "cadran/coral");
        if (tint && c.icon)
            cadran::List::recolor_icon(cell, nullptr, tint,
                                       c.tone == ui::Choice::Tone::DANGER ? "cadran/coral_soft"
                                       : c.tone == ui::Choice::Tone::WARN ? "cadran/amber_surf"
                                                                          : "cadran/teal_soft");
        const std::function<void()> run = c.run;
        cell->registerClickAction([run](aurore::View*) {
            aurore::Application::popActivity(aurore::TransitionAnimation::FADE, [run]() {
                if (run) aurore::sync(run);
            });
            return true;
        });
    }
}

void GameActivity::show_details(std::shared_ptr<const titledb::Details> d)
{
    this->details = d;
    const uint64_t base = library::base_of(this->page.title_id);
    // Its name, for a game known only by its title id.
    if (!d->name.empty() && this->page.name == title_id::text(this->page.title_id)) {
        this->title->setText(d->name);
        if (auto* applet = dynamic_cast<aurore::AppletFrame*>(this->getContentView())) applet->setTitle(d->name);
    }
    if (!d->intro.empty()) {
        intro->setText(d->intro);
        intro->setVisibility(aurore::Visibility::VISIBLE);
    }
    if (!d->description.empty()) {
        description->setText(d->description);
        description->setVisibility(aurore::Visibility::VISIBLE);
        about_header->setVisibility(aurore::Visibility::VISIBLE);
    }
    source->setVisibility(aurore::Visibility::VISIBLE);
    this->add_screens(d->screens);
    this->show_chips();
    this->fill_details_tab();

    // The games most like it, then more of its publisher and of its first
    // genre, from the caller.
    if (this->page.editions) this->add_others("playshelf/game/editions"_i18n, this->page.editions());
    if (this->page.similar) this->add_others("playshelf/game/similar"_i18n, this->page.similar());
    if (this->page.by_publisher && !d->publisher.empty())
        this->add_others(aurore::getStr("playshelf/game/more_publisher", d->publisher), this->page.by_publisher(d->publisher));
    const auto genre = std::find_if(d->categories.begin(), d->categories.end(), [](const std::string& g) { return g != "Other"; });
    if (this->page.by_genre && genre != d->categories.end())
        this->add_others(aurore::getStr("playshelf/game/more_genre", nlib_flow::genre_name(*genre)), this->page.by_genre(*genre));

    // Its banner, over the page once it came.
    ui::Weak weak(this->alive);
    library_icons::picture(titledb::nlib_banner_url(base), 854, 480, [this, weak, base](const library_icons::Picture& p) {
        const ui::Alive held = weak.lock();
        if (!held || !*held) return;
        if (library_icons::show(banner, p)) {
            banner->setVisibility(aurore::Visibility::VISIBLE);
            this->over_banner();
            return;
        }
        // No banner from Nlib API: IGDB's wide art, by Atlas' name for it, asked of wsrv.nl (an
        // image cache in the European Union: it gets the console's address and the picture's,
        // IGDB never does). Only with config popular_online (the Atlas data) and atlas_pictures.
        if (!config::get().popular_online || !config::get().atlas_pictures) return;
        const library::GameSignals* sig = popular_flow::signals()->find(base);
        const std::string           url = sig ? library::art_url(*sig, 854, 480) : "";
        if (url.empty()) return;
        library_icons::picture(url, 854, 480, [this, weak](const library_icons::Picture& art) {
            const ui::Alive again = weak.lock();
            if (!again || !*again || !library_icons::show(banner, art)) return;
            banner->setVisibility(aurore::Visibility::VISIBLE);
            this->over_banner();
        });
    });
}

void GameActivity::over_banner()
{
    head->setPositionType(aurore::PositionType::ABSOLUTE);
    head->setPositionLeft(28);
    head->setPositionRight(28);
    head->setPositionBottom(22);
}

// Publisher · developer · year on top; its genres, how many play, the
// language PlayShelf speaks when the game has it, a demo, as chips.
void GameActivity::show_chips()
{
    const library::Facts f = nlib_flow::facts(this->page.title_id);
    std::string          line = f.publisher;
    if (!f.developer.empty() && f.developer != f.publisher) line += (line.empty() ? "" : " · ") + f.developer;
    if (f.release_date) line += (line.empty() ? "" : " · ") + std::to_string(f.release_date / 10000);
    if (!line.empty()) {
        this->meta->setText(line);
        this->meta->setVisibility(aurore::Visibility::VISIBLE);
    }
    this->chips->clearViews(true);
    auto add = [this](const std::string& text, const char* icon_name, const char* ink, const char* tint) {
        this->chips->addView(chip(text, icon_name, ink, tint));
    };
    for (const std::string& g : f.categories)
        if (g != "Other") add(nlib_flow::genre_name(g), "tag", "cadran/teal", "cadran/teal_soft");
    if (f.players == 1) add("playshelf/game/players_one"_i18n, "user", "cadran/text2", "cadran/surf2");
    else if (f.players > 1)
        add(aurore::getStr("playshelf/game/players_many", f.players), "user", "cadran/text2", "cadran/surf2");
    const std::string own = titledb::nlib_language(live_prefs::locale());
    if (std::ranges::contains(f.languages, own))
        add("playshelf/game/language_chip"_i18n, "language", "cadran/text2", "cadran/surf2");
    // Its score, tinted by how good it is: what one looks for before installing.
    if (const int score = score_of(this->page.title_id)) {
        add(aurore::getStr("playshelf/game/critics_score", score), "sparkle", score_ink(score), score_tint(score));
    }
    if (f.demo) add("playshelf/game/demo"_i18n, "flag", "cadran/amber", "cadran/amber_surf");
    kit::wrap(this->chips);
    this->chips->setVisibility(this->chips->getChildren().empty() ? aurore::Visibility::GONE : aurore::Visibility::VISIBLE);
}

// Details: its facts in titled sections (the game, what is said of it, where
// the console stands, where it is to be had, its ids), its eShop page as a
// QR code. A section with nothing to say is left out.
void GameActivity::fill_details_tab()
{
    facts->clearViews(true);
    const library::Facts           f = nlib_flow::facts(this->page.title_id);
    const library_flow::GameLines& l = this->page.lines;
    const library::GameSignals*    sig = popular_flow::signals()->find(this->page.title_id);
    // A row: its name and value; what A on it browses the Catalogue by ("publisher", "genre",
    // "series", "theme"), with the label and the filter's word of each of its values.
    struct Row
    {
        std::string                                      name, value, key;
        std::vector<std::pair<std::string, std::string>> picks;
        Row(std::string n, std::string v, std::string k = "", std::vector<std::pair<std::string, std::string>> p = {})
            : name(std::move(n)), value(std::move(v)), key(std::move(k)), picks(std::move(p))
        {
        }
    };
    using Rows = std::vector<Row>;
    // The Catalogue opened on the games that share a publisher, a genre, a series, a theme.
    const bool can_browse = !library::load_servers().empty();
    auto       browse = [](const std::string& key, const std::string& word) {
        library::Filter filter;
        if (key == "publisher") filter.publisher = word;
        else if (key == "genre") filter.genre = word;
        else if (key == "series") filter.series = word;
        else if (key == "theme") filter.theme = word;
        aurore::Application::popActivity(aurore::TransitionAnimation::FADE,
                                         [filter]() { aurore::sync([filter]() { CatalogueTab::browse(filter); }); });
    };
    // Two columns: the sections on the left, what is looked up elsewhere (the age rating, the
    // eShop page as a QR code) on the right.
    auto* columns = new aurore::Box(aurore::Axis::ROW);
    columns->setAlignItems(aurore::AlignItems::FLEX_START);
    facts->addView(columns);
    auto* left = new aurore::Box(aurore::Axis::COLUMN);
    left->setGrow(1);
    left->setShrink(1);
    left->setAlignItems(aurore::AlignItems::STRETCH);
    columns->addView(left);
    bool first = true;
    // A section: its title, its rows in a card, then what `extra` holds (chips).
    auto section = [&](const std::string& name_, const Rows& rows, const std::vector<aurore::Box*>& extra = {}) {
        const bool any = std::ranges::any_of(rows, [](const Row& r) { return !r.value.empty(); });
        if (!any && extra.empty()) return;
        auto* header = new aurore::Header();
        header->setTitle(name_);
        header->setMarginTop(first ? 0 : 24);
        header->setMarginBottom(8);
        first = false;
        left->addView(header);
        cadran::List::section(header);
        auto* box = new aurore::Box(aurore::Axis::COLUMN);
        box->setAlignItems(aurore::AlignItems::STRETCH);
        left->addView(box);
        if (any) {
            aurore::Box* card = cadran::List::rows_in(box);
            for (const Row& r : rows) {
                aurore::DetailCell* cell = fact(card, r.name, r.value);
                if (!cell || !can_browse || r.picks.empty()) continue;
                // A on it: the Catalogue by its value, or by the one picked among several.
                cell->detail->setThemeColor("textColor", "cadran/teal");
                const std::string key = r.key, row_name = r.name;
                const auto        picks = r.picks;
                cell->registerClickAction(
                    [this, browse, key, row_name, picks](aurore::View*) {
                        if (picks.size() == 1) {
                            browse(key, picks[0].second);
                            return true;
                        }
                        std::vector<std::string> labels;
                        for (const auto& p : picks) labels.push_back(p.first);
                        ui::pick(row_name, labels, 0, ui::guarded(this->alive, [browse, key, picks](int i) {
                                     if (i >= 0 && i < (int)picks.size()) browse(key, picks[i].second);
                                 }));
                        return true;
                    });
            }
        }
        for (aurore::Box* e : extra) box->addView(e);
    };
    auto join = [](const std::vector<std::string>& items, auto&& name) {
        std::string out;
        for (const std::string& i : items) out += (out.empty() ? "" : ", ") + name(i);
        return out;
    };
    auto traits = [](const std::vector<library::Trait>& items) {
        std::string out;
        for (const library::Trait& t : items)
            if (!t.label.empty()) out += (out.empty() ? "" : ", ") + t.label;
        return out;
    };

    // The game: who made it, when, what kind; its languages and age rating as chips.
    Rows game;
    game.emplace_back("playshelf/game/publisher"_i18n, f.publisher, "publisher",
                      std::vector<std::pair<std::string, std::string>>(f.publisher.empty() ? 0 : 1, {f.publisher, f.publisher}));
    game.emplace_back("playshelf/game/developer"_i18n, f.developer);
    game.emplace_back("playshelf/game/released"_i18n, date_text(f.release_date));
    std::vector<std::pair<std::string, std::string>> genre_picks;
    for (const std::string& g : f.categories) genre_picks.emplace_back(nlib_flow::genre_name(g), g);
    game.emplace_back("playshelf/game/genres"_i18n, join(f.categories, [](const std::string& g) { return nlib_flow::genre_name(g); }),
                      "genre", genre_picks);
    // Its series (Atlas, from Wikidata) and its IGDB themes, by their English names.
    if (sig) {
        auto picks_of = [](const std::vector<library::Trait>& items) {
            std::vector<std::pair<std::string, std::string>> out;
            for (const library::Trait& t : items)
                if (!t.label.empty()) out.emplace_back(t.label, t.label);
            return out;
        };
        game.emplace_back("playshelf/game/series"_i18n, traits(sig->series), "series", picks_of(sig->series));
        game.emplace_back("playshelf/game/themes"_i18n, traits(sig->themes), "theme", picks_of(sig->themes));
    }
    if (f.players) game.emplace_back("playshelf/game/players"_i18n, std::to_string(f.players));
    // The other stores it is sold on (Atlas, from Wikidata): their names only.
    if (const uint8_t stores = popular_flow::also()->of(library::base_of(this->page.title_id)))
        game.emplace_back("playshelf/game/also_on"_i18n, library::store_names(stores));
    if (f.demo) game.emplace_back("playshelf/game/demo"_i18n, "playshelf/game/yes"_i18n);
    std::vector<aurore::Box*> pill_rows;
    if (!f.languages.empty()) {
        const std::string  own = titledb::nlib_language(live_prefs::locale());
        std::vector<Pill> pills;
        for (const std::string& code : f.languages) pills.push_back({language_name(code), code == own});
        std::ranges::stable_partition(pills, [](const Pill& p) { return p.on; });   // the one PlayShelf speaks first
        pill_rows.push_back(chip_block("playshelf/game/languages"_i18n, pills, "cadran/text2", "cadran/surf2"));
    }
    section("playshelf/game/section_game"_i18n, game, pill_rows);

    // What is said of it, as tiles: critics (Atlas, from Wikidata: OpenCritic's mostly), IGDB's
    // players from enough of them, Steam's, and where it stands among the games Atlas scores.
    std::vector<aurore::Box*> tiles;
    auto tile = [&](int score, const std::string& value, const std::string& caption, const std::string& sub) {
        tiles.push_back(score_tile(value, caption, sub, score_ink(score), score_tint(score)));
    };
    if (const int critics = popular_flow::of(this->page.title_id).critics)
        tile(critics, aurore::getStr("playshelf/game/critics_score", critics), "playshelf/game/critics"_i18n, "");
    if (sig && sig->igdb.count >= PLAYERS_TRUSTED)
        tile((int)sig->igdb.rating, aurore::getStr("playshelf/game/critics_score", (int)sig->igdb.rating),
             "playshelf/game/players_rating"_i18n, aurore::getStr("playshelf/game/tile_ratings", sig->igdb.count));
    if (sig) {
        const uint64_t votes = sig->steam.positive + sig->steam.negative;
        if (votes) {
            const int share = (int)(sig->steam.positive * 100 / votes);
            tile(share, std::format("{} %", share), "playshelf/game/steam"_i18n, aurore::getStr("playshelf/game/tile_reviews", votes));
        }
    }
    if (const library::Renown r = popular_flow::of(this->page.title_id); r.score > 0) {
        const int below = popular_flow::share_below(r.score);
        tiles.push_back(score_tile(std::format("{} %", below), "playshelf/game/renown_rank"_i18n, "playshelf/game/tile_renown"_i18n,
                                   "cadran/text2", "cadran/surf2"));
    }
    std::vector<aurore::Box*> tile_rows;
    if (!tiles.empty()) {
        auto* row = new aurore::Box(aurore::Axis::ROW);
        for (aurore::Box* t : tiles) row->addView(t);
        kit::wrap(row);
        tile_rows.push_back(row);
    }
    section("playshelf/game/section_reviews"_i18n, {}, tile_rows);


    // The console: what it has of it, how much it played it (core/play).
    Rows console;
    if (l.installed) console.emplace_back("playshelf/game/installed"_i18n, aurore::getStr("playshelf/library/update", l.installed_version));
    if (const auto games = installed_flow::games())
        for (const installed_flow::Game& g : *games) {
            if (g.game.id != this->page.title_id || !g.play_s) continue;
            std::string played = installed_flow::play_text(g.play_s);
            played += " · " + (g.launches == 1 ? "playshelf/game/launches_one"_i18n : aurore::getStr("playshelf/game/launches", g.launches));
            console.emplace_back("playshelf/game/play_time"_i18n, played);
            console.emplace_back("playshelf/game/last_played"_i18n, since_text(g.last_played));
            break;
        }
    // The room it takes beside the room the console has (what an install would use).
    std::vector<aurore::Box*> bars;
    if (const uint64_t need = l.base_size + l.updates_size + l.dlcs_size) {
        const installed_flow::Space space = installed_flow::space();
        const bool                  over = space.known && need > space.free;
        const std::string           caption =
            !space.known ? aurore::getStr("playshelf/game/space_need", library_flow::size_text(need))
            : over       ? aurore::getStr("playshelf/game/space_over", library_flow::size_text(need), library_flow::size_text(space.free))
                         : aurore::getStr("playshelf/game/space_fits", library_flow::size_text(need), library_flow::size_text(space.free));
        bars.push_back(size_bar({{"playshelf/game/size_base"_i18n, l.base_size, "cadran/teal"},
                                 {"playshelf/game/size_updates"_i18n, l.updates_size, "cadran/blue"},
                                 {"playshelf/game/size_dlcs"_i18n, l.dlcs_size, "cadran/violet"}},
                                space.known ? space.free : 0, caption, over ? "cadran/coral" : "cadran/text3"));
    }
    section("playshelf/game/section_console"_i18n, console, bars);

    // The servers that have it, each with what it has of it.
    Rows servers;
    for (const library_flow::Source& src : library_flow::sources(this->page.title_id)) servers.emplace_back(src.server, src.what);
    section("playshelf/game/sources"_i18n, servers);

    // Its ids, last: for whoever looks it up elsewhere.
    Rows ids;
    if (this->page.title_id) ids.emplace_back("playshelf/game/title_id"_i18n, title_id::text(this->page.title_id));
    if (f.nsu_id) ids.emplace_back("playshelf/game/eshop_id"_i18n, std::to_string(f.nsu_id));
    section("playshelf/game/section_ids"_i18n, ids);

    // The right column: its eShop page as a QR code, its age rating as chips (nothing, no column).
    if (f.nsu_id || !f.rating.empty()) {
        auto* right = new aurore::Box(aurore::Axis::COLUMN);
        right->setWidth(280);
        right->setShrink(0);
        right->setMarginLeft(24);
        right->setAlignItems(aurore::AlignItems::STRETCH);
        columns->addView(right);
        if (f.nsu_id)
            right->addView(kit::qr(eshop_url(f.nsu_id), 200, "playshelf/game/eshop"_i18n, "playshelf/game/eshop_caption"_i18n));
        if (!f.rating.empty()) {
            std::vector<Pill> pills;
            for (const std::string& r : f.rating) pills.push_back({r, false});
            right->addView(chip_block("playshelf/game/rating"_i18n, pills, "cadran/amber", "cadran/amber_surf"));
        }
    }
}

// Its screenshots, three a row: each a tile that A shows on the whole
// screen; the tiles there at once, their pictures as they come.
void GameActivity::add_screens(int count)
{
    if (count <= 0 || !screens_box->getChildren().empty()) return;
    const uint64_t base = library::base_of(this->page.title_id);
    screens_header->setVisibility(aurore::Visibility::VISIBLE);
    aurore::Box* row = nullptr;
    for (int n = 1; n <= count; n++) {
        if (!row || (int)row->getChildren().size() >= SHOTS_PER_ROW) {
            row = new aurore::Box(aurore::Axis::ROW);
            row->setMarginBottom(16);
            screens_box->addView(row);
        }
        auto* tile = new aurore::Box(aurore::Axis::ROW);
        tile->setWidth(SHOT_W);
        tile->setHeight(SHOT_H);
        tile->setMarginRight(16);
        tile->setCornerRadius(10);
        tile->setHighlightCornerRadius(12);
        tile->setThemeColor("backgroundColor", "cadran/well");
        tile->setAlignItems(aurore::AlignItems::CENTER);
        tile->setJustifyContent(aurore::JustifyContent::CENTER);
        tile->setFocusable(true);
        auto* image = new aurore::Image();
        image->setWidth(SHOT_W);
        image->setHeight(SHOT_H);
        image->setCornerRadius(10);
        image->setScalingType(aurore::ImageScalingType::FIT);
        tile->addView(image);
        tile->registerClickAction([base, count, n](aurore::View*) {
            aurore::Application::pushActivity(new ShotActivity(base, count, n), aurore::TransitionAnimation::FADE);
            return true;
        });
        row->addView(tile);
        ui::Weak weak(this->alive);
        library_icons::picture(titledb::nlib_screen_url(base, n), SHOT_W, SHOT_H, [weak, image](const library_icons::Picture& p) {
            const ui::Alive held = weak.lock();
            if (held && *held) library_icons::show(image, p);
        });
    }
}

// A row of more games like it, scrolled sideways: each a cover that A
// opens.
void GameActivity::add_others(const std::string& title_text, std::vector<Other> others)
{
    if (others.empty()) return;
    if (others.size() > MAX_OTHERS) others.resize(MAX_OTHERS);
    auto* header = new aurore::Header();
    header->setTitle(title_text);
    header->setMarginTop(24);
    header->setMarginBottom(8);
    others_box->addView(header);
    cadran::List::section(header);
    auto* frame = new aurore::HScrollingFrame();
    frame->setHeight(GameTile::WIDTH + 64);
    auto* row = new aurore::Box(aurore::Axis::ROW);
    frame->addView(row);
    others_box->addView(frame);
    for (Other& o : others) {
        auto* tile = new GameTile();
        tile->set_name(o.name);
        tile->set_detail("");
        const std::function<void()> open = o.open;
        tile->registerClickAction([open](aurore::View*) {
            if (open) open();
            return true;
        });
        row->addView(tile);
        if (o.icon) {
            ui::Weak weak(this->alive);
            o.icon([weak, tile](const library_icons::Picture& p) {
                const ui::Alive held = weak.lock();
                if (held && *held && p) tile->set_icon(p->rgba.data(), p->width, p->height);
            });
        }
    }
}
