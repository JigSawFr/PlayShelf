// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/game_activity.hpp"

#include <algorithm>
#include <ctime>
#include <utility>

#include <fmt/format.h>

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/ui/cadran_list.hpp>
#include <aurore/cadran/ui/live_prefs.hpp>
#include <aurore/cadran/view/cadran_icon.hpp>

#include "action/installed_flow.hpp"
#include "action/library_icons.hpp"
#include "action/nlib_flow.hpp"
#include "action/popular_flow.hpp"
#include "action/prices_flow.hpp"
#include "action/wishlist.hpp"
#include "library/catalog.hpp"
#include "ui/ui.hpp"
#include "util/title_id.hpp"
#include "view/cover_placeholder.hpp"
#include "view/game_tile.hpp"

using namespace aurore::literals;

namespace
{
namespace kit = cadran::kit;

constexpr float ICON = 112;
constexpr int   SHOT_W = 320, SHOT_H = 180;    // a screenshot's tile
constexpr int   SHOTS_PER_ROW = 3;
// A screenshot shown alone: 16:9 above its counter, the picture made that
// size (a view wider than its picture would draw its edges stretched).
constexpr int    FULL_H = 660, FULL_W = FULL_H * 16 / 9;
constexpr size_t MAX_OTHERS = 12;   // games in a row of more like it

enum Tab : size_t { OVERVIEW, UPDATES, DLCS, DETAILS };

constexpr float HERO_RADIUS = 16;   // the banner's corners

// The banner: its picture, its lower part fading into the page for the
// head laid over it to read on any picture.
class Banner : public aurore::Image
{
  public:
    Banner()
    {
        this->setHeight(360);
        this->setCornerRadius(HERO_RADIUS);
        this->setScalingType(aurore::ImageScalingType::FILL);
        this->setClipsToBounds(true);
        this->setVisibility(aurore::Visibility::GONE);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, aurore::Style style,
              aurore::FrameContext* ctx) override
    {
        aurore::Image::draw(vg, x, y, width, height, style, ctx);
        NVGcolor solid = aurore::Application::getTheme()["aurore/background"];
        solid.a = 0.96f * this->getAlpha();
        NVGcolor clear = solid;
        clear.a = 0.0f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, width, height, HERO_RADIUS);
        nvgFillPaint(vg, nvgLinearGradient(vg, x, y + height * 0.2f, x, y + height * 0.85f, clear, solid));
        nvgFill(vg);
    }
};

// A chip: its mark and its text, on a tint of its ink.
aurore::Box* chip(const std::string& text, const char* icon_name, const char* ink, const char* tint)
{
    auto* box = new aurore::Box(aurore::Axis::ROW);
    box->setAlignItems(aurore::AlignItems::CENTER);
    box->setHeight(32);
    box->setPaddingLeft(12);
    box->setPaddingRight(15);
    box->setCornerRadius(16);
    box->setThemeColor("backgroundColor", tint);
    box->setMarginRight(8);
    box->setMarginBottom(6);
    CadranIcon* mark = kit::icon(icon_name, 16, ink);
    mark->setMarginRight(7);
    box->addView(mark);
    aurore::Label* l = kit::label(text, 15, ink);
    l->setSingleLine(true);
    box->addView(l);
    return box;
}

// Where the console stands on a tab's items: a mark in a round of its tint,
// a line in the display face, a line under it.
aurore::Box* status_card(const std::string& title, const std::string& line, const char* icon_name, const char* ink,
                       const char* tint)
{
    auto* card = new aurore::Box(aurore::Axis::ROW);
    card->setAlignItems(aurore::AlignItems::CENTER);
    card->setPadding(16, 20, 16, 16);
    card->setCornerRadius(16);
    card->setThemeColor("backgroundColor", "cadran/surf");
    card->setThemeColor("borderColor", "cadran/line");
    card->setBorderThickness(1);
    card->setMarginBottom(16);
    auto* round = new aurore::Box(aurore::Axis::ROW);
    round->setWidth(48);
    round->setHeight(48);
    round->setShrink(0);
    round->setCornerRadius(24);
    round->setThemeColor("backgroundColor", tint);
    round->setAlignItems(aurore::AlignItems::CENTER);
    round->setJustifyContent(aurore::JustifyContent::CENTER);
    round->setMarginRight(16);
    round->addView(kit::icon(icon_name, 24, ink));
    card->addView(round);
    auto* text = new aurore::Box(aurore::Axis::COLUMN);
    text->setGrow(1);
    text->setShrink(1);
    text->addView(kit::display(title, 21, ink));
    if (!line.empty()) {
        aurore::Label* under = kit::label(line, 16, "cadran/text3");
        under->setMarginTop(4);
        text->addView(under);
    }
    card->addView(text);
    return card;
}

// A game's language in the language PlayShelf speaks ("ko" -> "coréen"),
// Nlib API's code itself for one PlayShelf has no word for.
std::string language_name(const std::string& code)
{
    for (const char* known : titledb::NLIB_LANGUAGES)
        if (code == known) return aurore::getStr("playshelf/languages/" + code);
    return code;
}

// "3 March 2017", in the language's order.
std::string date_text(uint32_t ymd)
{
    if (!ymd) return "";
    const unsigned y = ymd / 10000, m = ymd / 100 % 100, d = ymd % 100;
    if (m < 1 || m > 12) return std::to_string(y);
    return aurore::getStr("playshelf/game/date", d, aurore::getStr(fmt::format("cadran/months/{}", m)), y, m);
}

// Its eShop page: ec.nintendo.com sends a phone to the shop of the country
// PlayShelf's language is spoken in.
std::string eshop_url(uint64_t nsu_id)
{
    static const std::pair<const char*, const char*> COUNTRIES[] = {
        { "en-US", "US" }, { "fr", "FR" }, { "fr-CA", "CA" }, { "de", "DE" },      { "es", "ES" },      { "es-419", "MX" },
        { "it", "IT" },    { "nl", "NL" }, { "pt", "PT" },    { "pt-BR", "BR" },   { "ru", "RU" },      { "ja", "JP" },
        { "ko", "KR" },    { "zh-Hans", "HK" }, { "zh-Hant", "HK" },
    };
    const std::string locale = live_prefs::locale();
    const char*       country = "US";
    for (const auto& [l, c] : COUNTRIES)
        if (locale == l) country = c;
    return fmt::format("https://ec.nintendo.com/apps/{}/{}", nsu_id, country);
}

// One screenshot on the whole screen, black around it: ◀ ▶ the others, B
// back to the page.
class ShotActivity : public aurore::Activity
{
  public:
    ShotActivity(uint64_t base_id, int shots, int first) : base(base_id), count(shots), at(first) {}

    aurore::View* createContentView() override
    {
        auto* box = new aurore::Box(aurore::Axis::COLUMN);
        box->setWidth(aurore::View::AUTO);
        box->setHeight(aurore::View::AUTO);
        box->setGrow(1);
        box->setAlignItems(aurore::AlignItems::CENTER);
        box->setJustifyContent(aurore::JustifyContent::CENTER);
        box->setBackgroundColor(nvgRGB(0, 0, 0));
        box->setFocusable(true);
        box->setHideHighlight(true);
        this->image = new aurore::Image();
        this->image->setWidth(FULL_W);
        this->image->setHeight(FULL_H);
        this->image->setScalingType(aurore::ImageScalingType::FIT);
        box->addView(this->image);
        this->counter = kit::label("", 18, "cadran/text3");
        this->counter->setMarginTop(12);
        box->addView(this->counter);
        box->registerAction("hints/back"_i18n, aurore::BUTTON_B, [](aurore::View*) {
            aurore::Application::popActivity(aurore::TransitionAnimation::FADE);
            return true;
        });
        auto step = [this](int by) {
            return [this, by](aurore::View*) {
                this->at = (this->at - 1 + by + this->count) % this->count + 1;
                this->load();
                return true;
            };
        };
        box->registerAction("", aurore::BUTTON_LEFT, step(-1), true, true);
        box->registerAction("", aurore::BUTTON_RIGHT, step(1), true, true);
        box->registerAction("", aurore::BUTTON_LB, step(-1), true);
        box->registerAction("", aurore::BUTTON_RB, step(1), true);
        this->load();
        return box;
    }

    ~ShotActivity() override { *this->alive = false; }

  private:
    uint64_t     base;
    int          count, at;
    aurore::Image* image = nullptr;
    aurore::Label* counter = nullptr;
    ui::Alive    alive = ui::make_alive();

    void load()
    {
        this->counter->setText(fmt::format("{} / {}", this->at, this->count));
        const int wanted = this->at;
        ui::Weak  weak(this->alive);
        library_icons::picture(titledb::nlib_screen_url(this->base, wanted), FULL_W, FULL_H,
                               [this, weak, wanted](const library_icons::Picture& p) {
                                   const ui::Alive held = weak.lock();
                                   if (held && *held && wanted == this->at) library_icons::show(this->image, p);
                               });
    }
};

// A row of a card of facts: its name, its value (nothing when it has none).
void fact(aurore::Box* card, const std::string& name, const std::string& value)
{
    if (value.empty()) return;
    auto* cell = new aurore::DetailCell();
    cell->setText(name);
    cell->setDetailText(value);
    card->addView(cell);
    cadran::List::row(cell);
    cell->detail->setMaxWidth(640);
}
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
    aurore::Label* facts_line = kit::label(this->page.lines.facts, 16, "cadran/text3");
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
    if (this->page.similar) this->add_others("playshelf/game/similar"_i18n, this->page.similar());
    if (this->page.by_publisher && !d->publisher.empty())
        this->add_others(aurore::getStr("playshelf/game/more_publisher", d->publisher), this->page.by_publisher(d->publisher));
    const auto genre = std::find_if(d->categories.begin(), d->categories.end(), [](const std::string& g) { return g != "Other"; });
    if (this->page.by_genre && genre != d->categories.end())
        this->add_others(aurore::getStr("playshelf/game/more_genre", nlib_flow::genre_name(*genre)), this->page.by_genre(*genre));

    // Its banner, over the page once it came.
    ui::Weak weak(this->alive);
    library_icons::picture(titledb::nlib_banner_url(base), 854, 480, [this, weak](const library_icons::Picture& p) {
        const ui::Alive held = weak.lock();
        if (!held || !*held || !library_icons::show(banner, p)) return;
        banner->setVisibility(aurore::Visibility::VISIBLE);
        this->over_banner();
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
    if (f.demo) add("playshelf/game/demo"_i18n, "flag", "cadran/amber", "cadran/amber_surf");
    kit::wrap(this->chips);
    this->chips->setVisibility(this->chips->getChildren().empty() ? aurore::Visibility::GONE : aurore::Visibility::VISIBLE);
}

// Details: its facts in a card, its eShop page as a QR code.
void GameActivity::fill_details_tab()
{
    facts->clearViews(true);
    const library::Facts           f = nlib_flow::facts(this->page.title_id);
    const library_flow::GameLines& l = this->page.lines;
    auto*                          holder = new aurore::Box(aurore::Axis::COLUMN);
    holder->setAlignItems(aurore::AlignItems::STRETCH);
    facts->addView(holder);
    aurore::Box* card = cadran::List::rows_in(holder);
    if (this->page.title_id) fact(card, "playshelf/game/title_id"_i18n, title_id::text(this->page.title_id));
    if (f.nsu_id) fact(card, "playshelf/game/eshop_id"_i18n, std::to_string(f.nsu_id));
    fact(card, "playshelf/game/publisher"_i18n, f.publisher);
    fact(card, "playshelf/game/developer"_i18n, f.developer);
    fact(card, "playshelf/game/released"_i18n, date_text(f.release_date));
    std::string genres;
    for (const std::string& g : f.categories) genres += (genres.empty() ? "" : ", ") + nlib_flow::genre_name(g);
    fact(card, "playshelf/game/genres"_i18n, genres);
    if (f.players) fact(card, "playshelf/game/players"_i18n, std::to_string(f.players));
    std::string languages;
    for (const std::string& code : f.languages) languages += (languages.empty() ? "" : ", ") + language_name(code);
    fact(card, "playshelf/game/languages"_i18n, languages);
    std::string rating;
    for (const std::string& r : f.rating) rating += (rating.empty() ? "" : ", ") + r;
    fact(card, "playshelf/game/rating"_i18n, rating);
    // Its series and its Steam audience (PlayInsights, from Wikidata and SteamSpy).
    if (const library::GameSignals* sig = popular_flow::signals()->find(this->page.title_id)) {
        std::string series;
        for (const library::Trait& t : sig->series)
            if (!t.label.empty()) series += (series.empty() ? "" : ", ") + t.label;
        fact(card, "playshelf/game/series"_i18n, series);
        const uint64_t reviews = sig->steam.positive + sig->steam.negative;
        if (reviews)
            fact(card, "playshelf/game/steam"_i18n,
                 aurore::getStr("playshelf/game/steam_value", (int)(sig->steam.positive * 100 / reviews), reviews));
    }
    // Its eShop price in the player's country (action/prices_flow); on sale,
    // the regular price, the sale's and its last day, and whether it is the
    // lowest yet; then the lowest it has had, when lower than today's.
    if (const library::Price* price = prices_flow::of(this->page.title_id)) {
        const std::string country = prices_flow::country();
        const std::string regular = library::price_text(price->regular, country);
        std::string       line;
        if (!price->on_sale()) line = price->regular > 0 ? regular : "playshelf/game/free"_i18n;
        else if (price->until)
            line = aurore::getStr("playshelf/game/price_sale_until", regular, library::price_text(price->sale, country),
                                  price->discount(), date_text(price->until));
        else
            line = aurore::getStr("playshelf/game/price_sale", regular, library::price_text(price->sale, country), price->discount());
        if (price->on_sale() && price->at_lowest()) line += " · " + "playshelf/game/price_lowest_now"_i18n;
        fact(card, "playshelf/game/price"_i18n, line);
        const std::string since = prices_flow::book() ? prices_flow::book()->since() : "";
        if (price->lowest > 0 && price->lowest < price->now() - 0.004 && price->lowest_day && since.size() == 10) {
            const uint32_t first = (uint32_t)std::stoul(since.substr(0, 4) + since.substr(5, 2) + since.substr(8, 2));
            fact(card, aurore::getStr("playshelf/game/lowest", date_text(first)),
                 aurore::getStr("playshelf/game/lowest_value", library::price_text(price->lowest, country), date_text(price->lowest_day)));
        }
    }
    // What critics gave it (PlayInsights, from Wikidata: OpenCritic's mostly).
    if (const int critics = popular_flow::of(this->page.title_id).critics)
        fact(card, "playshelf/game/critics"_i18n, aurore::getStr("playshelf/game/critics_score", critics));
    if (f.demo) fact(card, "playshelf/game/demo"_i18n, "playshelf/game/yes"_i18n);
    if (l.base_size) fact(card, "playshelf/game/size_base"_i18n, library_flow::size_text(l.base_size));
    if (l.updates_size) fact(card, "playshelf/game/size_updates"_i18n, library_flow::size_text(l.updates_size));
    if (l.dlcs_size) fact(card, "playshelf/game/size_dlcs"_i18n, library_flow::size_text(l.dlcs_size));
    if (l.installed) fact(card, "playshelf/game/installed"_i18n, aurore::getStr("playshelf/library/update", l.installed_version));
    // How much the console played it (core/play).
    if (const auto games = installed_flow::games())
        for (const installed_flow::Game& g : *games) {
            if (g.game.id != this->page.title_id || !g.play_s) continue;
            std::string played = installed_flow::play_text(g.play_s);
            played += " · " + (g.launches == 1 ? "playshelf/game/launches_one"_i18n : aurore::getStr("playshelf/game/launches", g.launches));
            fact(card, "playshelf/game/play_time"_i18n, played);
            if (g.last_played) {
                const std::time_t when = (std::time_t)g.last_played;
                std::tm           day{};
                localtime_r(&when, &day);
                fact(card, "playshelf/game/last_played"_i18n,
                     date_text((uint32_t)((day.tm_year + 1900) * 10000 + (day.tm_mon + 1) * 100 + day.tm_mday)));
            }
            break;
        }
    // The servers that have it, each with what it has of it.
    const std::vector<library_flow::Source> sources = library_flow::sources(this->page.title_id);
    if (!sources.empty()) {
        auto* header = new aurore::Header();
        header->setTitle("playshelf/game/sources"_i18n);
        header->setMarginTop(24);
        header->setMarginBottom(8);
        facts->addView(header);
        cadran::List::section(header);
        auto* box = new aurore::Box(aurore::Axis::COLUMN);
        box->setAlignItems(aurore::AlignItems::STRETCH);
        facts->addView(box);
        aurore::Box* rows = cadran::List::rows_in(box);
        for (const library_flow::Source& src : sources) fact(rows, src.server, src.what);
    }
    if (f.nsu_id) {
        aurore::Box* qr = kit::qr(eshop_url(f.nsu_id), 200, "playshelf/game/eshop"_i18n, "playshelf/game/eshop_caption"_i18n);
        qr->setMarginTop(24);
        facts->addView(qr);
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
