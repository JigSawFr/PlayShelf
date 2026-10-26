// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/game_activity.hpp"

#include <algorithm>
#include <utility>

#include <fmt/format.h>

#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/cadran_list.hpp>
#include <borealis/cadran/ui/live_prefs.hpp>
#include <borealis/cadran/view/cadran_icon.hpp>

#include "action/library_icons.hpp"
#include "action/nlib_flow.hpp"
#include "library/catalog.hpp"
#include "ui/ui.hpp"
#include "view/game_tile.hpp"

using namespace brls::literals;

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

// A texture that came after its view went: freed.
void drop(int texture)
{
    if (texture) nvgDeleteImage(brls::Application::getNVGContext(), texture);
}

// A game's language in the language PlayShelf speaks ("ko" -> "coréen"),
// Nlib API's code itself for one PlayShelf has no word for.
std::string language_name(const std::string& code)
{
    for (const char* known : titledb::NLIB_LANGUAGES)
        if (code == known) return brls::getStr("playshelf/languages/" + code);
    return code;
}

// "3 March 2017", in the language's order.
std::string date_text(uint32_t ymd)
{
    if (!ymd) return "";
    const unsigned y = ymd / 10000, m = ymd / 100 % 100, d = ymd % 100;
    if (m < 1 || m > 12) return std::to_string(y);
    return brls::getStr("playshelf/game/date", d, brls::getStr(fmt::format("cadran/months/{}", m)), y, m);
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
class ShotActivity : public brls::Activity
{
  public:
    ShotActivity(uint64_t base_id, int shots, int first) : base(base_id), count(shots), at(first) {}

    brls::View* createContentView() override
    {
        auto* box = new brls::Box(brls::Axis::COLUMN);
        box->setWidth(brls::View::AUTO);
        box->setHeight(brls::View::AUTO);
        box->setGrow(1);
        box->setAlignItems(brls::AlignItems::CENTER);
        box->setJustifyContent(brls::JustifyContent::CENTER);
        box->setBackgroundColor(nvgRGB(0, 0, 0));
        box->setFocusable(true);
        box->setHideHighlight(true);
        this->image = new brls::Image();
        this->image->setWidth(FULL_W);
        this->image->setHeight(FULL_H);
        this->image->setScalingType(brls::ImageScalingType::FIT);
        this->image->setFreeTexture(true);
        box->addView(this->image);
        this->counter = kit::label("", 18, "cadran/text3");
        this->counter->setMarginTop(12);
        box->addView(this->counter);
        box->registerAction("hints/back"_i18n, brls::BUTTON_B, [](brls::View*) {
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        });
        auto step = [this](int by) {
            return [this, by](brls::View*) {
                this->at = (this->at - 1 + by + this->count) % this->count + 1;
                this->load();
                return true;
            };
        };
        box->registerAction("", brls::BUTTON_LEFT, step(-1), true, true);
        box->registerAction("", brls::BUTTON_RIGHT, step(1), true, true);
        box->registerAction("", brls::BUTTON_LB, step(-1), true);
        box->registerAction("", brls::BUTTON_RB, step(1), true);
        this->load();
        return box;
    }

    ~ShotActivity() override { *this->alive = false; }

  private:
    uint64_t     base;
    int          count, at;
    brls::Image* image = nullptr;
    brls::Label* counter = nullptr;
    ui::Alive    alive = ui::make_alive();

    void load()
    {
        this->counter->setText(fmt::format("{} / {}", this->at, this->count));
        const int wanted = this->at;
        ui::Weak  weak(this->alive);
        library_icons::picture(titledb::nlib_screen_url(this->base, wanted), FULL_W, FULL_H,
                               [this, weak, wanted](int texture, int, int) {
                                   const ui::Alive held = weak.lock();
                                   if (!held || !*held || wanted != this->at || !texture) return drop(texture);
                                   this->image->innerSetImage(texture);
                               });
    }
};

// A row of a card of facts: its name, its value (nothing when it has none).
void fact(brls::Box* card, const std::string& name, const std::string& value)
{
    if (value.empty()) return;
    auto* cell = new brls::DetailCell();
    cell->setText(name);
    cell->setDetailText(value);
    card->addView(cell);
    cadran::List::row(cell);
    cell->detail->setMaxWidth(640);
}
}   // namespace

void GameActivity::show(Page page)
{
    brls::Application::pushActivity(new GameActivity(std::move(page)));
}

GameActivity::~GameActivity()
{
    *this->alive = false;
}

void GameActivity::onContentAvailable()
{
    if (auto* applet = dynamic_cast<brls::AppletFrame*>(this->getContentView())) applet->setTitle(this->page.name);
    cadran::List::section(screens_header);
    cadran::List::section(about_header);
    this->add_head();
    this->add_tabs();
    this->fill_details_tab();

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
    auto* cover = new brls::Box(brls::Axis::ROW);
    cover->setWidth(ICON);
    cover->setHeight(ICON);
    cover->setShrink(0);
    cover->setCornerRadius(12);
    cover->setBackgroundColor(kit::color("cadran/well"));
    cover->setAlignItems(brls::AlignItems::CENTER);
    cover->setJustifyContent(brls::JustifyContent::CENTER);
    cover->setMarginRight(24);
    CadranIcon* placeholder = kit::icon("gamepad", 48, "cadran/text3");
    cover->addView(placeholder);
    this->icon = new brls::Image();
    this->icon->setWidth(ICON);
    this->icon->setHeight(ICON);
    this->icon->setCornerRadius(12);
    this->icon->setScalingType(brls::ImageScalingType::FIT);
    this->icon->setFreeTexture(true);
    this->icon->setVisibility(brls::Visibility::GONE);
    cover->addView(this->icon);
    head->addView(cover);

    auto* text = new brls::Box(brls::Axis::COLUMN);
    text->setGrow(1);
    text->setShrink(1);
    this->title = kit::display(this->page.name, 28);
    text->addView(this->title);
    this->meta = kit::label("", 17, "cadran/text2");
    this->meta->setMarginTop(6);
    this->meta->setVisibility(brls::Visibility::GONE);
    text->addView(this->meta);
    brls::Label* facts_line = kit::label(this->page.lines.facts, 16, "cadran/text3");
    facts_line->setMarginTop(4);
    text->addView(facts_line);
    if (!this->page.note.empty()) {
        brls::Label* note = kit::label(this->page.note, 16, "cadran/amber");
        note->setMarginTop(6);
        text->addView(note);
    }
    this->chips = new brls::Box(brls::Axis::ROW);
    this->chips->setMarginTop(10);
    this->chips->setVisibility(brls::Visibility::GONE);
    text->addView(this->chips);
    head->addView(text);

    if (this->page.icon) {
        ui::Weak weak(this->alive);
        this->page.icon([this, weak, placeholder](int texture) {
            const ui::Alive held = weak.lock();
            if (!held || !*held || !texture) return drop(texture);
            this->icon->innerSetImage(texture);
            this->icon->setVisibility(brls::Visibility::VISIBLE);
            placeholder->setVisibility(brls::Visibility::GONE);
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
        { UPDATES, brls::getStr("playshelf/game/tab_updates", l.update_count) },
        { DLCS, brls::getStr("playshelf/game/tab_dlcs", l.dlc_count) },
        { DETAILS, "playshelf/game/tab_details"_i18n },
    };
    this->panes = { overview, updates, dlcs, facts };
    for (const auto& [tab, label] : TABS) {
        auto* box = new brls::Box(brls::Axis::ROW);
        box->setPadding(10, 22, 10, 22);
        box->setMarginRight(10);
        box->setCornerRadius(22);
        box->setHighlightCornerRadius(24);
        box->setFocusable(true);
        box->addView(kit::label(label, 18, "cadran/text2"));
        const size_t index = tab;
        box->registerClickAction([this, index](brls::View*) {
            this->select(index, true);
            return true;
        });
        const bool shown = tab == OVERVIEW || tab == DETAILS || (tab == UPDATES && l.update_count) || (tab == DLCS && l.dlc_count);
        box->setVisibility(shown ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        tab_bar->addView(box);
        this->tabs.push_back(box);
    }
    // L and R: the tab before and after, of those shown.
    auto step = [this](int by) {
        return [this, by](brls::View*) {
            size_t at = this->current;
            for (size_t n = 0; n < this->tabs.size(); n++) {
                at = (at + this->tabs.size() + (size_t)by) % this->tabs.size();
                if (this->tabs[at]->getVisibility() == brls::Visibility::VISIBLE) break;
            }
            this->select(at, true);
            return true;
        };
    };
    this->getContentView()->registerAction("playshelf/game/tab_previous"_i18n, brls::BUTTON_LB, step(-1));
    this->getContentView()->registerAction("playshelf/game/tab_next"_i18n, brls::BUTTON_RB, step(1));

    // Overview: everything missing and the base game, then the caller's.
    std::vector<ui::Choice> main = l.main;
    main.insert(main.end(), l.more.begin(), l.more.end());
    this->add_lines(lines_box, main);

    // Updates: the versions installed and newest, then the lines.
    if (l.update_count) {
        std::string line = l.installed ? brls::getStr("playshelf/game/version_installed", l.installed_version)
                                       : "playshelf/game/not_installed"_i18n;
        if (l.latest_version) line += " · " + brls::getStr("playshelf/game/version_latest", l.latest_version);
        brls::Label* summary = kit::label(line, 18, "cadran/text2");
        summary->setMarginBottom(14);
        updates->addView(summary);
        this->add_lines(updates, l.updates);
    }
    // Add-ons: how many the console has, then the lines.
    if (l.dlc_count) {
        brls::Label* summary = kit::label(brls::getStr("playshelf/game/dlcs_installed", l.dlcs_installed, l.dlc_count), 18,
                                          l.dlcs_installed == l.dlc_count ? "cadran/teal" : "cadran/text2");
        summary->setMarginBottom(14);
        dlcs->addView(summary);
        this->add_lines(dlcs, l.dlcs);
    }
    this->select(OVERVIEW, false);
}

void GameActivity::select(size_t tab, bool focus)
{
    if (tab >= this->tabs.size()) return;
    this->current = tab;
    brls::Theme theme = brls::Application::getTheme();
    for (size_t i = 0; i < this->tabs.size(); i++) {
        const bool on = i == tab;
        this->tabs[i]->setBackgroundColor(theme[on ? "cadran/teal_soft" : "cadran/surf2"]);
        if (auto* label = dynamic_cast<brls::Label*>(this->tabs[i]->getChildren()[0]))
            label->setTextColor(theme[on ? "cadran/teal" : "cadran/text2"]);
        this->panes[i]->setVisibility(on ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }
    if (focus) brls::Application::giveFocus(this->tabs[tab]);
}

// The lines in a card, as the sheet had them: A runs one, the page gone
// first.
void GameActivity::add_lines(brls::Box* pane, const std::vector<ui::Choice>& lines)
{
    if (lines.empty()) return;
    brls::Theme theme = brls::Application::getTheme();
    auto*       holder = new brls::Box(brls::Axis::COLUMN);
    holder->setAlignItems(brls::AlignItems::STRETCH);
    pane->addView(holder);
    brls::Box* card = cadran::List::rows_in(holder);
    for (const ui::Choice& c : lines) {
        auto* cell = new brls::DetailCell();
        cell->setText(c.label);
        cell->setDetailText(c.detail);
        card->addView(cell);
        cadran::List::row(cell, c.icon);
        cell->detail->setSingleLine(true);
        cell->detail->setVisibility(c.detail.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        const char* tint = c.tone == ui::Choice::Tone::ON     ? "cadran/teal"
                         : c.tone == ui::Choice::Tone::WARN   ? "cadran/amber"
                         : c.tone == ui::Choice::Tone::DANGER ? "cadran/coral"
                                                              : nullptr;
        cell->detail->setTextColor(theme[tint ? tint : "cadran/text2"]);
        if (c.tone == ui::Choice::Tone::DANGER) cell->title->setTextColor(theme["cadran/coral"]);
        if (tint && c.icon)
            cadran::List::recolor_icon(cell, nullptr, tint,
                                       c.tone == ui::Choice::Tone::DANGER ? "cadran/coral_soft"
                                       : c.tone == ui::Choice::Tone::WARN ? "cadran/amber_surf"
                                                                          : "cadran/teal_soft");
        const std::function<void()> run = c.run;
        cell->registerClickAction([run](brls::View*) {
            brls::Application::popActivity(brls::TransitionAnimation::FADE, [run]() {
                if (run) brls::sync(run);
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
    if (!d->name.empty() && this->page.name == fmt::format("{:016X}", this->page.title_id)) {
        this->title->setText(d->name);
        if (auto* applet = dynamic_cast<brls::AppletFrame*>(this->getContentView())) applet->setTitle(d->name);
    }
    if (!d->intro.empty()) {
        intro->setText(d->intro);
        intro->setVisibility(brls::Visibility::VISIBLE);
    }
    if (!d->description.empty()) {
        description->setText(d->description);
        description->setVisibility(brls::Visibility::VISIBLE);
        about_header->setVisibility(brls::Visibility::VISIBLE);
    }
    source->setVisibility(brls::Visibility::VISIBLE);
    this->add_screens(d->screens);
    this->show_chips();
    this->fill_details_tab();

    // More of its publisher and of its first genre, from the caller.
    if (this->page.by_publisher && !d->publisher.empty())
        this->add_others(brls::getStr("playshelf/game/more_publisher", d->publisher), this->page.by_publisher(d->publisher));
    const auto genre = std::find_if(d->categories.begin(), d->categories.end(), [](const std::string& g) { return g != "Other"; });
    if (this->page.by_genre && genre != d->categories.end())
        this->add_others(brls::getStr("playshelf/game/more_genre", nlib_flow::genre_name(*genre)), this->page.by_genre(*genre));

    // Its banner, over the page once it came.
    ui::Weak weak(this->alive);
    library_icons::picture(titledb::nlib_banner_url(base), 854, 480, [this, weak](int texture, int, int) {
        const ui::Alive held = weak.lock();
        if (!held || !*held || !texture) return drop(texture);
        banner->innerSetImage(texture);
        banner->setVisibility(brls::Visibility::VISIBLE);
    });
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
        this->meta->setVisibility(brls::Visibility::VISIBLE);
    }
    this->chips->clearViews(true);
    auto add = [this](const std::string& text, const char* dot) {
        brls::Box* p = kit::pill(text, dot);
        p->setMarginRight(8);
        p->setMarginBottom(6);
        this->chips->addView(p);
    };
    for (const std::string& g : f.categories)
        if (g != "Other") add(nlib_flow::genre_name(g), "cadran/teal");
    if (f.players == 1) add("playshelf/game/players_one"_i18n, "cadran/text3");
    else if (f.players > 1) add(brls::getStr("playshelf/game/players_many", f.players), "cadran/text3");
    const std::string own = titledb::nlib_language(live_prefs::locale());
    if (std::find(f.languages.begin(), f.languages.end(), own) != f.languages.end())
        add("playshelf/game/language_chip"_i18n, "cadran/text3");
    if (f.demo) add("playshelf/game/demo"_i18n, "cadran/amber");
    kit::wrap(this->chips);
    this->chips->setVisibility(this->chips->getChildren().empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
}

// Details: its facts in a card, its eShop page as a QR code.
void GameActivity::fill_details_tab()
{
    facts->clearViews(true);
    const library::Facts           f = nlib_flow::facts(this->page.title_id);
    const library_flow::GameLines& l = this->page.lines;
    auto*                          holder = new brls::Box(brls::Axis::COLUMN);
    holder->setAlignItems(brls::AlignItems::STRETCH);
    facts->addView(holder);
    brls::Box* card = cadran::List::rows_in(holder);
    if (this->page.title_id) fact(card, "playshelf/game/title_id"_i18n, fmt::format("{:016X}", this->page.title_id));
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
    if (f.demo) fact(card, "playshelf/game/demo"_i18n, "playshelf/game/yes"_i18n);
    if (l.base_size) fact(card, "playshelf/game/size_base"_i18n, library_flow::size_text(l.base_size));
    if (l.updates_size) fact(card, "playshelf/game/size_updates"_i18n, library_flow::size_text(l.updates_size));
    if (l.dlcs_size) fact(card, "playshelf/game/size_dlcs"_i18n, library_flow::size_text(l.dlcs_size));
    if (l.installed) fact(card, "playshelf/game/installed"_i18n, brls::getStr("playshelf/library/update", l.installed_version));
    if (f.nsu_id) {
        brls::Box* qr = kit::qr(eshop_url(f.nsu_id), 200, "playshelf/game/eshop"_i18n, "playshelf/game/eshop_caption"_i18n);
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
    screens_header->setVisibility(brls::Visibility::VISIBLE);
    brls::Box* row = nullptr;
    for (int n = 1; n <= count; n++) {
        if (!row || (int)row->getChildren().size() >= SHOTS_PER_ROW) {
            row = new brls::Box(brls::Axis::ROW);
            row->setMarginBottom(16);
            screens_box->addView(row);
        }
        auto* tile = new brls::Box(brls::Axis::ROW);
        tile->setWidth(SHOT_W);
        tile->setHeight(SHOT_H);
        tile->setMarginRight(16);
        tile->setCornerRadius(10);
        tile->setHighlightCornerRadius(12);
        tile->setBackgroundColor(kit::color("cadran/well"));
        tile->setAlignItems(brls::AlignItems::CENTER);
        tile->setJustifyContent(brls::JustifyContent::CENTER);
        tile->setFocusable(true);
        auto* image = new brls::Image();
        image->setWidth(SHOT_W);
        image->setHeight(SHOT_H);
        image->setCornerRadius(10);
        image->setScalingType(brls::ImageScalingType::FIT);
        image->setFreeTexture(true);
        tile->addView(image);
        tile->registerClickAction([base, count, n](brls::View*) {
            brls::Application::pushActivity(new ShotActivity(base, count, n), brls::TransitionAnimation::FADE);
            return true;
        });
        row->addView(tile);
        ui::Weak weak(this->alive);
        library_icons::picture(titledb::nlib_screen_url(base, n), SHOT_W, SHOT_H, [weak, image](int texture, int, int) {
            const ui::Alive held = weak.lock();
            if (!held || !*held || !texture) return drop(texture);
            image->innerSetImage(texture);
        });
    }
}

// A row of more games like it, scrolled sideways: each a cover that A
// opens.
void GameActivity::add_others(const std::string& title_text, std::vector<Other> others)
{
    if (others.empty()) return;
    if (others.size() > MAX_OTHERS) others.resize(MAX_OTHERS);
    auto* header = new brls::Header();
    header->setTitle(title_text);
    header->setMarginTop(24);
    header->setMarginBottom(8);
    others_box->addView(header);
    cadran::List::section(header);
    auto* frame = new brls::HScrollingFrame();
    frame->setHeight(GameTile::WIDTH + 64);
    auto* row = new brls::Box(brls::Axis::ROW);
    frame->addView(row);
    others_box->addView(frame);
    for (Other& o : others) {
        auto* tile = new GameTile();
        tile->set_name(o.name);
        tile->set_detail("");
        const std::function<void()> open = o.open;
        tile->registerClickAction([open](brls::View*) {
            if (open) open();
            return true;
        });
        row->addView(tile);
        if (o.icon) {
            ui::Weak weak(this->alive);
            o.icon([weak, tile](int texture) {
                const ui::Alive held = weak.lock();
                if (!held || !*held || !texture) return drop(texture);
                tile->set_icon_texture(texture, true);
            });
        }
    }
}
