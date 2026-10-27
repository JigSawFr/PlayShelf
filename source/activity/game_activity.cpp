// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/game_activity.hpp"

#include <utility>

#include <fmt/format.h>

#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/cadran_list.hpp>
#include <borealis/cadran/view/cadran_icon.hpp>

#include "action/library_icons.hpp"
#include "action/nlib_flow.hpp"
#include "library/catalog.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

namespace
{
namespace kit = cadran::kit;

constexpr float ICON = 112;
constexpr int   SHOT_W = 320, SHOT_H = 180;    // a screenshot's tile
constexpr int   SHOTS_PER_ROW = 3;
// A screenshot shown alone: 16:9 above its counter, the picture made that
// size (a view wider than its picture would draw its edges stretched).
constexpr int   FULL_H = 660, FULL_W = FULL_H * 16 / 9;

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
        const int  wanted = this->at;
        ui::Weak   weak(this->alive);
        library_icons::picture(titledb::nlib_screen_url(this->base, wanted), FULL_W, FULL_H,
                               [this, weak, wanted](const library_icons::Picture& p) {
                                   const ui::Alive held = weak.lock();
                                   if (held && *held && wanted == this->at) library_icons::show(this->image, p);
                               });
    }
};
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

    // Its icon, its name, its facts; what Nlib API knows of it under them.
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
    this->icon->setVisibility(brls::Visibility::GONE);
    cover->addView(this->icon);
    head->addView(cover);

    auto* text = new brls::Box(brls::Axis::COLUMN);
    text->setGrow(1);
    text->setShrink(1);
    this->title = kit::display(this->page.name, 28);
    text->addView(this->title);
    brls::Label* facts = kit::label(this->page.facts, 17, "cadran/text2");
    facts->setMarginTop(6);
    text->addView(facts);
    this->meta = kit::label("", 17, "cadran/text3");
    this->meta->setMarginTop(4);
    this->meta->setVisibility(brls::Visibility::GONE);
    text->addView(this->meta);
    if (!this->page.note.empty()) {
        brls::Label* note = kit::label(this->page.note, 17, "cadran/amber");
        note->setMarginTop(6);
        text->addView(note);
    }
    head->addView(text);

    if (this->page.icon) {
        ui::Weak weak(this->alive);
        this->page.icon([this, weak, placeholder](const library_icons::Picture& p) {
            const ui::Alive held = weak.lock();
            if (!held || !*held || !library_icons::show(this->icon, p)) return;
            this->icon->setVisibility(brls::Visibility::VISIBLE);
            placeholder->setVisibility(brls::Visibility::GONE);
        });
    }

    this->add_lines();

    // What Nlib API knows of it: asked once the page is up.
    if (this->page.title_id) {
        ui::Weak weak(this->alive);
        nlib_flow::details(this->page.title_id, [this, weak](std::shared_ptr<const titledb::Details> d) {
            const ui::Alive held = weak.lock();
            if (held && *held && d) this->show_details(*d);
        });
    }
}

// The lines, by group: a kicker over each group's card, as the sheet had
// them.
void GameActivity::add_lines()
{
    brls::Theme  theme = brls::Application::getTheme();
    brls::Box*   card = nullptr;
    std::string  group;
    brls::View*  first = nullptr;
    for (const ui::Choice& c : this->page.lines) {
        if (!card || c.group != group) {
            group = c.group;
            if (!group.empty()) {
                auto* header = new brls::Header();
                header->setTitle(group);
                header->setMarginTop(card ? 20 : 0);
                header->setMarginBottom(8);
                lines_box->addView(header);
                cadran::List::section(header);
            }
            auto* holder = new brls::Box(brls::Axis::COLUMN);
            holder->setAlignItems(brls::AlignItems::STRETCH);
            if (group.empty() && card) holder->setMarginTop(20);
            lines_box->addView(holder);
            card = cadran::List::rows_in(holder);
        }
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
        // Chosen: the page goes, then the line's action.
        const std::function<void()> run = c.run;
        cell->registerClickAction([run](brls::View*) {
            brls::Application::popActivity(brls::TransitionAnimation::FADE, [run]() {
                if (run) brls::sync(run);
            });
            return true;
        });
        if (!first) first = cell;
    }
    if (first) brls::Application::giveFocus(first);
}

void GameActivity::show_details(const titledb::Details& d)
{
    const uint64_t base = library::base_of(this->page.title_id);
    // Its name, for a game known only by its title id.
    if (!d.name.empty() && this->page.name == fmt::format("{:016X}", this->page.title_id)) {
        this->title->setText(d.name);
        if (auto* applet = dynamic_cast<brls::AppletFrame*>(this->getContentView())) applet->setTitle(d.name);
    }
    // Publisher · year · its first three categories.
    std::string line = d.publisher;
    if (!d.release_date.empty()) line += (line.empty() ? "" : " · ") + d.release_date.substr(0, 4);
    if (!d.categories.empty()) {
        std::string cats;
        for (size_t i = 0; i < d.categories.size() && i < 3; i++) cats += (i ? ", " : "") + d.categories[i];
        line += (line.empty() ? "" : " · ") + cats;
    }
    if (!line.empty()) {
        this->meta->setText(line);
        this->meta->setVisibility(brls::Visibility::VISIBLE);
    }
    if (!d.intro.empty()) {
        intro->setText(d.intro);
        intro->setVisibility(brls::Visibility::VISIBLE);
    }
    if (!d.description.empty()) {
        description->setText(d.description);
        description->setVisibility(brls::Visibility::VISIBLE);
        about_header->setVisibility(brls::Visibility::VISIBLE);
    }
    source->setVisibility(brls::Visibility::VISIBLE);
    this->add_screens(d.screens);

    // Its banner, over the page once it came.
    ui::Weak weak(this->alive);
    library_icons::picture(titledb::nlib_banner_url(base), 854, 480, [this, weak](const library_icons::Picture& p) {
        const ui::Alive held = weak.lock();
        if (!held || !*held || !library_icons::show(banner, p)) return;
        banner->setVisibility(brls::Visibility::VISIBLE);
    });
}

// Its screenshots, three a row: each a tile that A shows on the whole
// screen; the tiles there at once, their pictures as they come.
void GameActivity::add_screens(int count)
{
    if (count <= 0) return;
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
        tile->addView(image);
        tile->registerClickAction([base, count, n](brls::View*) {
            brls::Application::pushActivity(new ShotActivity(base, count, n), brls::TransitionAnimation::FADE);
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
