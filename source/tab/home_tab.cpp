// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/home_tab.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>

#include <fmt/format.h>

#include <borealis/cadran/ui/cadran_frame.hpp>
#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/cadran_list.hpp>
#include <borealis/cadran/ui/cadran_modal.hpp>

#include "action/download_flow.hpp"
#include "action/game_page.hpp"
#include "action/library_icons.hpp"
#include "action/nlib_flow.hpp"
#include "activity/tabs.hpp"
#include "library/installed.hpp"
#include "library/servers.hpp"
#include "tab/library_tab.hpp"
#include "titledb/nlib.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "view/banner_card.hpp"
#include "view/game_tile.hpp"

using namespace brls::literals;
namespace kit = cadran::kit;

namespace
{
constexpr size_t BANNERS = 8;    // games in a row of banners
constexpr size_t COVERS = 12;    // games in a row of covers
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

// Today as YYYYMMDD, a year ago.
uint32_t year_ago()
{
    const std::time_t now = std::time(nullptr);
    std::tm           today{};
    gmtime_r(&now, &today);
    return (uint32_t)((today.tm_year + 1900 - 1) * 10000 + (today.tm_mon + 1) * 100 + today.tm_mday);
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

// The catalogues read in this run, the servers' order.
std::vector<std::shared_ptr<const library_flow::Loaded>> catalogues()
{
    std::vector<std::shared_ptr<const library_flow::Loaded>> out;
    for (const library::Server& s : library::load_servers())
        if (auto l = library_flow::loaded(s.id)) out.push_back(std::move(l));
    return out;
}

uint32_t date_of(const library::Title& t)
{
    const uint32_t known = nlib_flow::facts(t.id).release_date;
    return known ? known : t.release_date;
}
}   // namespace

HomeTab::HomeTab()
    : TabBase("xml/tab/home.xml")
{
    const cadran::List margins(this);
    (void)margins;
    // The queue's changes: the downloads' tile.
    this->downloads_listener = download_flow::listen(ui::guarded(this->alive, [this]() {
        if (this->shown()) this->build(false);
    }));
    // Facts came: the rows that sort by them (not under the focus).
    this->nlib_listener = nlib_flow::listen(ui::guarded(this->alive, [this]() {
        if (this->shown() && !inside(brls::Application::getCurrentFocus(), content)) this->build(false);
    }));
}

HomeTab::~HomeTab()
{
    download_flow::unlisten(this->downloads_listener);
    nlib_flow::unlisten(this->nlib_listener);
}

void HomeTab::refresh()
{
    download_flow::show_pill();
    this->games = installed_flow::games();
    this->build(false);
    installed_flow::read(ui::guarded(this->alive, [this](const std::shared_ptr<const std::vector<installed_flow::Game>>& g) {
        this->games = g;
        this->build(false);
        this->read_next();
    }));
}

void HomeTab::read_next()
{
    if (library_flow::loading()) return;
    for (const library::Server& s : library::load_servers()) {
        if (library_flow::loaded(s.id) || this->tried.count(s.id)) continue;
        this->tried.insert(s.id);
        library_flow::load(s, ui::guarded(this->alive, [this](const std::shared_ptr<const library_flow::Loaded>&, const std::string&) {
            this->build(false);
            this->read_next();
        }));
        return;
    }
}

void HomeTab::build(bool force)
{
    const auto all = catalogues();
    const auto installed = library_flow::installed();
    const size_t game_count = this->games ? this->games->size() : 0;

    // The games of the servers, each once (its first server's).
    std::vector<Pick>  every;
    std::set<uint64_t> seen;
    size_t             server_games = 0;
    for (const auto& l : all)
        for (size_t i = 0; i < l->titles.size(); i++) {
            const library::Title& t = l->titles[i];
            if (!t.id || !seen.insert(t.id).second) continue;
            every.push_back({ l, i });
            server_games++;
        }

    // The console's games with something to get, and what they lack.
    // The console's games with something to get, the most played first, and
    // what they lack; the genres of its games, each weighed by how many have
    // it and how much they were played.
    std::vector<std::pair<uint64_t, Pick>> getting;
    size_t                                 updates = 0, dlcs = 0;
    std::map<std::string, double>          genres;
    if (this->games)
        for (const installed_flow::Game& g : *this->games) {
            const double weight = 1.0 + std::log2(1.0 + (double)g.play_s / 3600.0);
            for (const std::string& c : nlib_flow::facts(g.game.id).categories)
                if (c != "Other") genres[c] += weight;
            for (const auto& l : all) {
                const long t = library::title_of(l->titles, g.game.id);
                if (t < 0) continue;
                const library::TitleState st = library::state_of(l->titles[(size_t)t], l->fetch.catalog, *installed);
                updates += st.update_available ? 1 : 0;
                dlcs += st.dlcs_missing;
                if (st.update_available || st.dlcs_missing) getting.push_back({ g.play_s, { l, (size_t)t } });
                break;
            }
        }
    std::stable_sort(getting.begin(), getting.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<Pick> to_get;
    for (const auto& g : getting) {
        if (to_get.size() >= BANNERS) break;
        to_get.push_back(g.second);
    }

    // The console's games played lately, the latest first.
    std::vector<const installed_flow::Game*> recent;
    if (this->games)
        for (const installed_flow::Game& g : *this->games)
            if (g.last_played) recent.push_back(&g);
    std::stable_sort(recent.begin(), recent.end(),
                     [](const installed_flow::Game* a, const installed_flow::Game* b) { return a->last_played > b->last_played; });
    if (recent.size() > COVERS) recent.resize(COVERS);

    // The servers' newest games the console has not.
    std::vector<Pick> newest;
    std::vector<Pick> fresh;
    for (const Pick& p : every) {
        const library::Title& t = p.data->titles[p.index];
        if (library::state_of(t, p.data->fetch.catalog, *installed).base || nlib_flow::facts(t.id).demo) continue;
        fresh.push_back(p);
        if (date_of(t)) newest.push_back(p);
    }
    std::stable_sort(newest.begin(), newest.end(), [](const Pick& a, const Pick& b) {
        return date_of(a.data->titles[a.index]) > date_of(b.data->titles[b.index]);
    });
    if (newest.size() > BANNERS) newest.resize(BANNERS);

    // Games like the console's: by the genres its games have, weighed by
    // how many have each; newer first among equals.
    std::vector<std::pair<double, Pick>> scored;
    double                               weight = 0;
    for (const auto& [g, n] : genres) weight += n;
    if (weight > 0)
        for (const Pick& p : fresh) {
            const library::Title& t = p.data->titles[p.index];
            double                score = 0;
            for (const std::string& c : nlib_flow::facts(t.id).categories) {
                const auto it = genres.find(c);
                if (it != genres.end()) score += it->second / weight;
            }
            if (score > 0) scored.emplace_back(score + date_of(t) / 1e12, p);
        }
    std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<Pick> like;
    for (const auto& s : scored) {
        if (like.size() >= COVERS) break;
        like.push_back(s.second);
    }

    size_t downloads = 0;
    for (const download::Job& j : download_flow::jobs())
        if (j.state != download::State::Done) downloads++;

    // What the rows show, to build them again only when it changed (the
    // focus may be on one of them).
    std::string key = fmt::format("{} {} {} {} {} {}|", game_count, server_games, updates, dlcs, downloads, all.size());
    for (const auto* list : { &to_get, &newest, &like })
        for (const Pick& p : *list) key += fmt::format("{:x},", p.data->titles[p.index].id);
    for (const installed_flow::Game* g : recent) key += fmt::format("{:x}.{},", g->game.id, g->last_played);
    if (!force && key == this->built_key) return;
    this->built_key = key;

    const bool had_focus = inside(brls::Application::getCurrentFocus(), content);
    if (had_focus) brls::Application::giveFocus(this);
    content->clearViews(true);
    this->cards.clear();
    this->tiles.clear();
    this->generation++;

    if (!game_count && library::load_servers().empty()) return this->add_welcome();
    this->add_hero(game_count, server_games);
    this->add_tiles(game_count, updates, dlcs, downloads);
    this->add_recent(recent, all);
    if (config::get().nlib_api) {
        this->add_banners("playshelf/installed/featured"_i18n, to_get, true);
        this->add_banners("playshelf/home/newest"_i18n, newest, false);
    }
    this->add_covers("playshelf/home/for_you"_i18n, "playshelf/home/for_you_line"_i18n, like);
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

void HomeTab::add_hero(size_t console_games, size_t server_games)
{
    content->addView(kit::display(greeting(), 34));
    std::string line = console_games == 1 ? "playshelf/installed/count_one"_i18n
                                          : brls::getStr("playshelf/installed/count", console_games);
    if (server_games) line += " · " + brls::getStr("playshelf/home/server_games", server_games);
    if (library_flow::loading()) line += " · " + "playshelf/library/loading"_i18n;
    brls::Label* under = kit::label(line, 18, "cadran/text2");
    under->setMarginTop(4);
    content->addView(under);
}

void HomeTab::add_tiles(size_t console_games, size_t updates, size_t dlcs, size_t downloads)
{
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setMargins(20, -6, 4, -6);
    // A number in the display face, its words under it; A runs `run` once
    // the click is over.
    auto tile = [row](size_t n, const std::string& words, const char* icon_name, const char* ink, std::function<void()> run) {
        auto* box = new brls::Box(brls::Axis::COLUMN);
        box->setGrow(1);
        box->setWidth(0);
        box->setPadding(16, 18, 16, 18);
        box->setMargins(0, 6, 0, 6);
        box->setCornerRadius(16);
        box->setHighlightCornerRadius(18);
        box->setBackgroundColor(kit::color("cadran/surf"));
        box->setBorderColor(kit::color("cadran/line"));
        box->setBorderThickness(1);
        box->setFocusable(true);
        auto* top = new brls::Box(brls::Axis::ROW);
        top->setAlignItems(brls::AlignItems::CENTER);
        top->addView(kit::display(std::to_string(n), 34, ink));
        auto* mark = kit::icon(icon_name, 20, ink);
        mark->setMarginLeft(10);
        top->addView(mark);
        box->addView(top);
        brls::Label* l = kit::label(words, 15, "cadran/text3");
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
    tile(console_games, "playshelf/home/stat_games"_i18n, "gamepad", "cadran/teal", library_with(library::Filter::State::Any));
    tile(updates, "playshelf/installed/stat_updates"_i18n, "download", updates ? "cadran/amber" : "cadran/text2",
         library_with(library::Filter::State::ToUpdate));
    tile(dlcs, "playshelf/installed/stat_dlcs"_i18n, "plus", dlcs ? "cadran/amber" : "cadran/text2",
         library_with(library::Filter::State::DlcsMissing));
    tile(downloads, "playshelf/home/stat_downloads"_i18n, "history", downloads ? "cadran/teal" : "cadran/text2",
         []() { ui::open_tab(tabs::downloads); });
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
    const uint32_t recent = year_ago();
    const int      gen = this->generation;
    for (const Pick& p : picks) {
        const library::Title&     t = p.data->titles[p.index];
        const library::Facts      f = nlib_flow::facts(t.id);
        const library::TitleState st = library::state_of(t, p.data->fetch.catalog, *installed);
        auto*                     card = new BannerCard(CARD_W, CARD_H);
        card->set_title(t.name);
        std::string line = f.publisher;
        if (date_of(t)) line += (line.empty() ? "" : " · ") + std::to_string(date_of(t) / 10000);
        for (const std::string& g : f.categories)
            if (g != "Other") {
                line += (line.empty() ? "" : " · ") + nlib_flow::genre_name(g);
                break;
            }
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
                               [weak = ui::Weak(this->alive), this, at, gen](int texture, int w, int h) {
                                   const ui::Alive held = weak.lock();
                                   if (held && *held && gen == this->generation && at < this->cards.size())
                                       this->cards[at]->set_picture(texture, w, h);
                                   else if (texture) nvgDeleteImage(brls::Application::getNVGContext(), texture);
                               });
    }
}

void HomeTab::add_covers(const std::string& title, const std::string& line, const std::vector<Pick>& picks)
{
    if (picks.empty()) return;
    brls::Box* row = this->add_row(title, line, GameTile::WIDTH + 64);
    const int  gen = this->generation;
    for (const Pick& p : picks) {
        const library::Title& t = p.data->titles[p.index];
        auto*                 tile = new GameTile();
        tile->set_name(t.name);
        const library::Facts f = nlib_flow::facts(t.id);
        std::string          detail;
        for (const std::string& g : f.categories)
            if (g != "Other") {
                detail = nlib_flow::genre_name(g);
                break;
            }
        tile->set_detail(detail.empty() ? library_flow::size_text(t.size) : detail);
        tile->set_detail_color(kit::color("cadran/text3"));
        const auto   data = p.data;
        const size_t index = p.index;
        tile->registerClickAction([data, index](brls::View*) {
            game_page::open(data, index);
            return true;
        });
        row->addView(tile);
        this->tiles.push_back(tile);
        const size_t at = this->tiles.size() - 1;
        auto shown = [weak = ui::Weak(this->alive), this, at, gen](int texture) {
            const ui::Alive held = weak.lock();
            if (held && *held && gen == this->generation && at < this->tiles.size()) this->tiles[at]->set_icon_texture(texture, true);
            else if (texture) nvgDeleteImage(brls::Application::getNVGContext(), texture);
        };
        if (!t.icon_url.empty()) library_icons::get(data, library::password_of(data->server), t.icon_url, shown, t.id);
        else library_icons::local(t.id, shown);
    }
}

void HomeTab::add_recent(const std::vector<const installed_flow::Game*>& recent,
                         const std::vector<std::shared_ptr<const library_flow::Loaded>>& all)
{
    if (recent.empty()) return;
    brls::Box* row = this->add_row("playshelf/home/recent"_i18n, "playshelf/home/recent_line"_i18n, GameTile::WIDTH + 64);
    const int  gen = this->generation;
    for (const installed_flow::Game* g : recent) {
        auto* tile = new GameTile();
        std::string name = g->name;
        if (name.empty()) name = nlib_flow::facts(g->game.id).name;
        if (name.empty()) name = fmt::format("{:016X}", g->game.id);
        tile->set_name(name);
        tile->set_detail(installed_flow::play_text(g->play_s));
        tile->set_detail_color(kit::color("cadran/teal"));
        // Its page from a catalogue that has it, else the Library.
        Pick found;
        for (const auto& l : all) {
            const long t = library::title_of(l->titles, g->game.id);
            if (t < 0) continue;
            found = { l, (size_t)t };
            break;
        }
        tile->registerClickAction([found](brls::View*) {
            if (found.data) game_page::open(found.data, found.index);
            else ui::open_tab(tabs::library);
            return true;
        });
        row->addView(tile);
        this->tiles.push_back(tile);
        const size_t at = this->tiles.size() - 1;
        auto shown = [weak = ui::Weak(this->alive), this, at, gen](int texture) {
            const ui::Alive held = weak.lock();
            if (held && *held && gen == this->generation && at < this->tiles.size()) this->tiles[at]->set_icon_texture(texture, true);
            else if (texture) nvgDeleteImage(brls::Application::getNVGContext(), texture);
        };
        // The console's own icon, else the title database's or Nlib API's.
        const auto bytes = g->icon;
        const int  texture = bytes ? nvgCreateImageMem(brls::Application::getNVGContext(), 0, (unsigned char*)bytes->data(),
                                                       (int)bytes->size())
                                   : 0;
        if (texture > 0) shown(texture);
        else library_icons::local(g->game.id, shown);
    }
}

brls::View* HomeTab::create()
{
    return new HomeTab();
}
