// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/home_tab.hpp"

#include <algorithm>
#include <ctime>

#include <fmt/format.h>

#include <borealis/cadran/ui/cadran_frame.hpp>
#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/cadran_list.hpp>
#include <borealis/cadran/ui/cadran_modal.hpp>

#include "action/download_flow.hpp"
#include "action/game_page.hpp"
#include "action/library_icons.hpp"
#include "action/nlib_flow.hpp"
#include "action/popular_flow.hpp"
#include "action/wishlist.hpp"
#include "activity/search_activity.hpp"
#include "activity/storage_activity.hpp"
#include "activity/tabs.hpp"
#include "library/seen.hpp"
#include "library/servers.hpp"
#include "tab/library_tab.hpp"
#include "titledb/nlib.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "view/banner_card.hpp"
#include "view/game_tile.hpp"
#include "view/tool_chip.hpp"

using namespace brls::literals;
namespace kit = cadran::kit;

namespace
{
constexpr size_t BANNERS = 8;    // games in a row of banners
constexpr float  CARD_W = 440, CARD_H = 220;

// The tab's room: the screen less the rail and the column's sides.
float room()
{
    return brls::Application::contentWidth - brls::Application::getStyle()["brls/tab_frame/sidebar_width"] -
           2 * brls::Application::getStyle()["brls/tab_frame/content_padding_sides"];
}

bool inside(brls::View* view, brls::View* parent)
{
    for (brls::View* v = view; v; v = v->getParent())
        if (v == parent) return true;
    return false;
}

// Today as YYYYMMDD, `days` from it.
uint32_t day(long days = 0)
{
    const std::time_t t = std::time(nullptr) + days * 86400;
    std::tm           local{};
    localtime_r(&t, &local);
    return (uint32_t)((local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday);
}

// The greeting of the hour.
std::string greeting()
{
    const std::time_t now = std::time(nullptr);
    std::tm           local{};
    localtime_r(&now, &local);
    if (local.tm_hour >= 5 && local.tm_hour < 12) return "playshelf/home/hello_morning"_i18n;
    if (local.tm_hour >= 12 && local.tm_hour < 18) return "playshelf/home/hello_afternoon"_i18n;
    return "playshelf/home/hello_evening"_i18n;
}

uint32_t date_of(const library::Title& t)
{
    const uint32_t known = nlib_flow::facts(t.id).release_date;
    return known ? known : t.release_date;
}

// "12 November 2026": a day, in the language's words.
std::string day_text(uint32_t date)
{
    const unsigned d = date % 100, m = date / 100 % 100, y = date / 10000;
    if (m < 1 || m > 12) return std::to_string(y);
    return brls::getStr("playshelf/game/date", d, brls::getStr(fmt::format("cadran/months/{}", m)), y, m);
}

// A game's first genre, in the language's words ("" when none known).
std::string genre_of(uint64_t id)
{
    for (const std::string& g : nlib_flow::facts(id).categories)
        if (g != "Other") return nlib_flow::genre_name(g);
    return "";
}

// What the rows show, to build them again only when it changed (the focus
// may be on one of them).
std::string key_of(const insights::Snapshot& s)
{
    std::string key = fmt::format("{} {} {} {} {} {} {} {} {} {}|", s.console_games, s.server_games, s.updates, s.dlcs,
                                  s.downloads, s.space.free / 100000000, s.loading, library::load_servers().size(),
                                  popular_flow::size(), config::get().nlib_api);
    for (const auto* list : { &s.to_get, &s.added, &s.popular, &s.trending, &s.month, &s.newest, &s.like }) {
        for (const insights::Pick& p : *list) key += fmt::format("{:x},", p.title().id);
        key += '|';
    }
    for (const insights::Played& p : s.recent) key += fmt::format("{:x}.{},", p.game.game.id, p.game.last_played);
    for (const insights::Wished& w : s.wished) key += fmt::format("{:x}.{}{},", w.id, w.found.data != nullptr, w.installed);
    for (const insights::Coming& c : s.coming) key += fmt::format("{:x},", c.id);
    return key;
}
}   // namespace

HomeTab::HomeTab()
    : TabBase("xml/tab/home.xml")
{
    const cadran::List margins(this);
    (void)margins;
    // What insights made again: the rows, unless the focus is on one of
    // them and only facts came (build() keeps the focus otherwise).
    this->insights_listener = insights::listen(ui::guarded(this->alive, [this]() {
        if (this->shown()) this->build(false);
    }));
}

HomeTab::~HomeTab()
{
    insights::unlisten(this->insights_listener);
}

void HomeTab::refresh()
{
    download_flow::show_pill();
    this->build(false);
    installed_flow::read(ui::guarded(this->alive, [](const std::shared_ptr<const std::vector<installed_flow::Game>>&) {
        insights::refresh();
    }));
}

void HomeTab::build(bool force)
{
    const auto             snap = insights::now();
    const insights::Snapshot& s = *snap;
    const std::string      key = key_of(s);
    if (!force && key == this->built_key) return;
    this->built_key = key;

    const bool had_focus = inside(brls::Application::getCurrentFocus(), content);
    if (had_focus) brls::Application::giveFocus(this);
    content->clearViews(true);
    this->cards.clear();
    this->tiles.clear();
    this->generation++;

    if (!s.console_games && library::load_servers().empty()) return this->add_welcome();
    this->add_hero(s);
    this->add_tiles(s);
    this->add_recent(s.recent);
    const bool online = config::get().nlib_api;
    if (online) this->add_banners("playshelf/installed/featured"_i18n, s.to_get, true);
    this->add_wished(s.wished);
    const uint32_t today = day();
    this->add_covers("playshelf/home/added"_i18n, "playshelf/home/added_line"_i18n, s.added, [today](const Pick& p) {
        const long days = std::max(0L, library::days_between(insights::first_seen(p), today));
        return Detail{ days == 0 ? "playshelf/home/added_today"_i18n
                       : days == 1 ? "playshelf/home/added_yesterday"_i18n
                                   : brls::getStr("playshelf/home/added_days", days),
                       "cadran/teal" };
    });
    if (online) this->add_banners("playshelf/home/popular"_i18n, s.popular, false);
    this->add_covers("playshelf/home/trending"_i18n, "playshelf/home/trending_line"_i18n, s.trending, [](const Pick& p) {
        return Detail{ brls::getStr("playshelf/home/trend", popular_flow::of(p.title().id).trend), "cadran/teal" };
    });
    if (online) {
        if (s.month.size() >= 3) this->add_banners("playshelf/home/month"_i18n, s.month, false);
        else this->add_banners("playshelf/home/newest"_i18n, s.newest, false);
    }
    this->add_covers("playshelf/home/for_you"_i18n, "playshelf/home/for_you_line"_i18n, s.like, [](const Pick& p) {
        const std::string g = genre_of(p.title().id);
        return Detail{ g.empty() ? library_flow::size_text(p.title().size) : g, "cadran/text3" };
    });
    this->add_coming(s.coming);
    if (had_focus)
        if (brls::View* first = content->getDefaultFocus()) brls::Application::giveFocus(first);
}

void HomeTab::add_welcome()
{
    kit::Empty empty = kit::empty("home");
    empty.title->setText("playshelf/home/welcome_title"_i18n);
    empty.text->setText("playshelf/home/welcome_text"_i18n);
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setMarginTop(20);
    row->setJustifyContent(brls::JustifyContent::CENTER);
    auto button = [row](const std::string& text, const std::function<void()>& run) {
        auto* b = new brls::Button();
        b->setText(text);
        cadran::style_button(b);
        b->setMarginLeft(8);
        b->setMarginRight(8);
        b->registerClickAction([run](brls::View*) {
            run();
            return true;
        });
        row->addView(b);
        kit::fit_button(b);
    };
    // Saved: the Catalogue, on its catalogue.
    auto saved = [](const std::string&) { ui::open_tab(tabs::catalogue); };
    button("playshelf/library/add_server"_i18n, [saved]() { library_flow::add(saved); });
    button("playshelf/library/discover"_i18n, [saved]() { library_flow::discover(saved); });
    kit::wrap(row);
    empty.box->addView(row);
    empty.box->setMarginTop(40);
    content->addView(empty.box);
}

void HomeTab::add_hero(const insights::Snapshot& s)
{
    // The greeting and its line at the left, the search at the right.
    auto* hero = new brls::Box(brls::Axis::ROW);
    hero->setAlignItems(brls::AlignItems::CENTER);
    auto* words = new brls::Box(brls::Axis::COLUMN);
    words->setGrow(1);
    words->addView(kit::display(greeting(), 34));
    std::string line = s.console_games == 1 ? "playshelf/installed/count_one"_i18n
                                            : brls::getStr("playshelf/installed/count", s.console_games);
    if (s.server_games) line += " · " + brls::getStr("playshelf/home/server_games", s.server_games);
    if (s.loading) line += " · " + "playshelf/library/loading"_i18n;
    brls::Label* under = kit::label(line, 18, "cadran/text2");
    under->setMarginTop(4);
    words->addView(under);
    hero->addView(words);
    auto* search = new ToolChip("eye", "playshelf/home/search"_i18n);
    search->set_value("playshelf/home/search_where"_i18n);
    search->registerClickAction([](brls::View*) {
        brls::sync([]() { SearchActivity::ask(); });
        return true;
    });
    hero->addView(search);
    content->addView(hero);
}

void HomeTab::add_tiles(const insights::Snapshot& s)
{
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setMargins(20, -6, 4, -6);
    // A number in the display face, its words under it; A runs `run` once
    // the click is over.
    auto tile = [row](const std::string& n, const std::string& tile_words, const char* icon_name, const char* ink,
                      std::function<void()> run) {
        auto* box = new brls::Box(brls::Axis::COLUMN);
        box->setGrow(1);
        box->setWidth(0);
        box->setPadding(16, 18, 16, 18);
        box->setMargins(0, 6, 0, 6);
        box->setCornerRadius(16);
        box->setHighlightCornerRadius(18);
        box->setThemeColor("backgroundColor", "cadran/surf");
        box->setThemeColor("borderColor", "cadran/line");
        box->setBorderThickness(1);
        box->setFocusable(true);
        auto* top = new brls::Box(brls::Axis::ROW);
        top->setAlignItems(brls::AlignItems::CENTER);
        top->addView(kit::display(n, 34, ink));
        auto* mark = kit::icon(icon_name, 20, ink);
        mark->setMarginLeft(10);
        top->addView(mark);
        box->addView(top);
        brls::Label* l = kit::label(tile_words, 15, "cadran/text3");
        l->setSingleLine(true);
        l->setMarginTop(2);
        box->addView(l);
        box->registerClickAction([run](brls::View*) {
            brls::sync(run);
            return true;
        });
        row->addView(box);
    };
    auto library_with = [](library::Filter::State state) {
        return [state]() {
            LibraryTab::preset(state);
            ui::open_tab(tabs::library);
        };
    };
    tile(std::to_string(s.console_games), "playshelf/home/stat_games"_i18n, "gamepad", "cadran/teal",
         library_with(library::Filter::State::Any));
    // The updates: all at once, or the Library with those games.
    const auto see_updates = library_with(library::Filter::State::ToUpdate);
    tile(std::to_string(s.updates), "playshelf/installed/stat_updates"_i18n, "download", s.updates ? "cadran/amber" : "cadran/text2",
         [see_updates, n = s.updates]() {
             if (!n) return see_updates();
             ui::pick_actions("playshelf/installed/stat_updates"_i18n,
                              { ui::Choice("playshelf/home/update_all"_i18n, []() { insights::update_all(); }, "download"),
                                ui::Choice("playshelf/home/see_list"_i18n, see_updates, "list") });
         });
    tile(std::to_string(s.dlcs), "playshelf/installed/stat_dlcs"_i18n, "plus", s.dlcs ? "cadran/amber" : "cadran/text2",
         library_with(library::Filter::State::DlcsMissing));
    tile(std::to_string(s.downloads), "playshelf/home/stat_downloads"_i18n, "history", s.downloads ? "cadran/teal" : "cadran/text2",
         []() { ui::open_tab(tabs::downloads); });
    if (s.space.known)
        tile(library_flow::size_text(s.space.free), "playshelf/home/stat_free"_i18n, "folder",
             insights::nearly_full(s.space) ? "cadran/amber" : "cadran/text2", []() { StorageActivity::open(); });
    content->addView(row);
}

brls::Box* HomeTab::add_row(const std::string& title, const std::string& line, float height)
{
    auto* head = new brls::Box(brls::Axis::ROW);
    head->setAlignItems(brls::AlignItems::FLEX_END);
    head->setMarginTop(26);
    head->setMarginLeft(8);
    head->addView(kit::display(title, 24));
    if (!line.empty()) {
        brls::Label* under = kit::label(line, 15, "cadran/text3");
        under->setMarginLeft(14);
        under->setMarginBottom(3);
        head->addView(under);
    }
    content->addView(head);
    auto* frame = new brls::HScrollingFrame();
    frame->setWidth(room());
    frame->setHeight(height);
    auto* row = new brls::Box(brls::Axis::ROW);
    frame->addView(row);
    content->addView(frame);
    return row;
}

void HomeTab::add_banners(const std::string& title, const std::vector<Pick>& picks, bool to_get)
{
    if (picks.empty()) return;
    brls::Box*     row = this->add_row(title, "", CARD_H + 16);
    const auto     installed = library_flow::installed();
    const uint32_t recent = day(-365);
    const int      gen = this->generation;
    size_t         shown = 0;
    for (const Pick& p : picks) {
        if (shown++ >= BANNERS) break;
        const library::Title&     t = p.title();
        const library::Facts      f = nlib_flow::facts(t.id);
        const library::TitleState st = library::state_of(t, p.data->fetch.catalog, *installed);
        auto*                     card = new BannerCard(CARD_W, CARD_H);
        card->set_title(t.name);
        std::string line = f.publisher;
        if (date_of(t)) line += (line.empty() ? "" : " · ") + std::to_string(date_of(t) / 10000);
        const std::string genre = genre_of(t.id);
        if (!genre.empty()) line += (line.empty() ? "" : " · ") + genre;
        card->set_line(line);
        if (to_get)
            card->set_kicker(st.update_available ? "playshelf/library/update_available"_i18n
                             : st.dlcs_missing == 1 ? "playshelf/installed/dlcs_missing_one"_i18n
                                                    : brls::getStr("playshelf/installed/dlcs_missing", st.dlcs_missing),
                             "cadran/amber");
        else if (date_of(t) >= recent) card->set_kicker("playshelf/library/new"_i18n, "cadran/teal");
        const auto   data = p.data;
        const size_t index = p.index;
        card->registerClickAction([data, index](brls::View*) {
            game_page::open(data, index);
            return true;
        });
        row->addView(card);
        this->cards.push_back(card);
        const size_t at = this->cards.size() - 1;
        library_icons::picture(titledb::nlib_banner_url(t.id), 854, 480,
                               [weak = ui::Weak(this->alive), this, at, gen](const library_icons::Picture& pic) {
                                   const ui::Alive held = weak.lock();
                                   if (pic && held && *held && gen == this->generation && at < this->cards.size())
                                       this->cards[at]->set_picture(pic->rgba.data(), pic->width, pic->height);
                               });
    }
}

void HomeTab::show_icon(const Pick& pick, uint64_t game_id)
{
    const int    gen = this->generation;
    const size_t at = this->tiles.size() - 1;
    auto shown = [weak = ui::Weak(this->alive), this, at, gen](const library_icons::Picture& pic) {
        const ui::Alive held = weak.lock();
        if (pic && held && *held && gen == this->generation && at < this->tiles.size())
            this->tiles[at]->set_icon(pic->rgba.data(), pic->width, pic->height);
    };
    if (pick.data && !pick.title().icon_url.empty())
        library_icons::get(pick.data, library::password_of(pick.data->server), pick.title().icon_url, shown, game_id);
    else
        library_icons::local(game_id, shown);
}

void HomeTab::add_covers(const std::string& title, const std::string& line, const std::vector<Pick>& picks,
                         const std::function<Detail(const Pick&)>& detail)
{
    if (picks.empty()) return;
    brls::Box* row = this->add_row(title, line, GameTile::WIDTH + 64);
    for (const Pick& p : picks) {
        const library::Title& t = p.title();
        auto*                 tile = new GameTile();
        tile->set_name(t.name);
        const Detail d = detail(p);
        tile->set_detail(d.text);
        tile->set_detail_color(d.ink);
        const auto   data = p.data;
        const size_t index = p.index;
        tile->registerClickAction([data, index](brls::View*) {
            game_page::open(data, index);
            return true;
        });
        row->addView(tile);
        this->tiles.push_back(tile);
        this->show_icon(p, t.id);
    }
}

GameTile* HomeTab::lone_tile(uint64_t game_id, const std::string& name, const Detail& detail)
{
    auto* tile = new GameTile();
    tile->set_name(name);
    tile->set_detail(detail.text);
    tile->set_detail_color(detail.ink);
    this->tiles.push_back(tile);
    this->show_icon({}, game_id);
    return tile;
}

void HomeTab::add_recent(const std::vector<insights::Played>& recent)
{
    if (recent.empty()) return;
    brls::Box* row = this->add_row("playshelf/home/recent"_i18n, "playshelf/home/recent_line"_i18n, GameTile::WIDTH + 64);
    const int  gen = this->generation;
    for (const insights::Played& p : recent) {
        auto* tile = new GameTile();
        tile->set_name(p.name);
        tile->set_detail(installed_flow::play_text(p.game.play_s));
        tile->set_detail_color("cadran/teal");
        // Its page from a catalogue that has it, else the Library.
        const Pick found = p.found;
        tile->registerClickAction([found](brls::View*) {
            if (found.data) game_page::open(found.data, found.index);
            else ui::open_tab(tabs::library);
            return true;
        });
        row->addView(tile);
        this->tiles.push_back(tile);
        const size_t at = this->tiles.size() - 1;
        auto shown = [weak = ui::Weak(this->alive), this, at, gen](const library_icons::Picture& pic) {
            const ui::Alive held = weak.lock();
            if (pic && held && *held && gen == this->generation && at < this->tiles.size())
                this->tiles[at]->set_icon(pic->rgba.data(), pic->width, pic->height);
        };
        // The console's own icon, else the title database's or Nlib API's.
        const uint64_t game_id = p.game.game.id;
        if (!p.game.icon) library_icons::local(game_id, shown);
        else
            library_icons::decode(p.game.icon, [shown, game_id](const library_icons::Picture& pic) {
                if (pic) shown(pic);
                else library_icons::local(game_id, shown);
            });
    }
}

void HomeTab::add_wished(const std::vector<insights::Wished>& wished)
{
    if (wished.empty()) return;
    brls::Box* row = this->add_row("playshelf/home/wishlist"_i18n, "playshelf/home/wishlist_line"_i18n, GameTile::WIDTH + 64);
    for (const insights::Wished& w : wished) {
        GameTile* tile = nullptr;
        if (w.found.data) {
            tile = new GameTile();
            tile->set_name(w.name);
            tile->set_detail(w.installed ? "playshelf/library/installed"_i18n : w.found.data->server.name);
            tile->set_detail_color(w.installed ? "cadran/text3" : "cadran/teal");
            const auto   data = w.found.data;
            const size_t index = w.found.index;
            tile->registerClickAction([data, index](brls::View*) {
                game_page::open(data, index);
                return true;
            });
            this->tiles.push_back(tile);
            this->show_icon(w.found, w.id);
        } else {
            // On no server yet: A takes it off the list.
            tile = this->lone_tile(w.id, w.name, { "playshelf/home/wish_waiting"_i18n, "cadran/text3" });
            const uint64_t    game_id = w.id;
            const std::string name = w.name;
            tile->registerClickAction([game_id, name](brls::View*) {
                ui::pick_actions(name, { ui::Choice("playshelf/wishlist/remove"_i18n, [game_id, name]() { wishlist::toggle(game_id, name); },
                                                    "trash") },
                                 "playshelf/home/wish_waiting_long"_i18n);
                return true;
            });
        }
        row->addView(tile);
    }
}

void HomeTab::add_coming(const std::vector<insights::Coming>& coming)
{
    if (coming.empty()) return;
    brls::Box* row = this->add_row("playshelf/home/coming"_i18n, "playshelf/home/coming_line"_i18n, GameTile::WIDTH + 64);
    for (const insights::Coming& c : coming) {
        GameTile* tile = this->lone_tile(c.id, c.name, { day_text(c.date), "cadran/teal" });
        if (wishlist::has(c.id)) tile->set_badge("heart", "cadran/pink");
        // A: on the wishlist (the bell says when a server has it), or off.
        const uint64_t    game_id = c.id;
        const std::string name = c.name;
        const std::string about = c.publisher.empty() ? day_text(c.date) : c.publisher + " · " + day_text(c.date);
        tile->registerClickAction([game_id, name, about](brls::View*) {
            const bool on = wishlist::has(game_id);
            ui::pick_actions(name,
                             { ui::Choice(on ? "playshelf/wishlist/remove"_i18n : "playshelf/wishlist/add"_i18n,
                                          [game_id, name]() { wishlist::toggle(game_id, name); }, "heart") },
                             about);
            return true;
        });
        row->addView(tile);
    }
}

brls::View* HomeTab::create()
{
    return new HomeTab();
}
