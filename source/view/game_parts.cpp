// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/game_parts.hpp"

#include <chrono>
#include <ctime>
#include <format>
#include <utility>

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/ui/cadran_list.hpp>
#include <aurore/cadran/ui/live_prefs.hpp>
#include <aurore/cadran/view/cadran_icon.hpp>

#include "action/library_icons.hpp"
#include "titledb/nlib.hpp"

using namespace aurore::literals;

namespace game_parts
{

namespace kit = cadran::kit;

namespace
{
constexpr float HERO_RADIUS = 16;   // the banner's corners
// A screenshot shown alone: 16:9 above its counter, the picture made that
// size (a view wider than its picture would draw its edges stretched).
constexpr int FULL_H = 660, FULL_W = FULL_H * 16 / 9;
}   // namespace

Banner::Banner()
{
    this->setHeight(360);
    this->setCornerRadius(HERO_RADIUS);
    this->setScalingType(aurore::ImageScalingType::FILL);
    this->setClipsToBounds(true);
    this->setVisibility(aurore::Visibility::GONE);
}

void Banner::draw(NVGcontext* vg, float x, float y, float width, float height, aurore::Style style, aurore::FrameContext* ctx)
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
    if (icon_name && *icon_name) {
        CadranIcon* mark = kit::icon(icon_name, 16, ink);
        mark->setMarginRight(7);
        box->addView(mark);
    } else {
        box->setPaddingLeft(15);
    }
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
    return aurore::getStr("playshelf/game/date", d, aurore::getStr(std::format("cadran/months/{}", m)), y, m);
}

// Its eShop page: ec.nintendo.com sends a phone to the shop of the country
// PlayShelf's language is spoken in.
std::string since_text(uint64_t when)
{
    if (!when) return "";
    namespace ch = std::chrono;
    const std::time_t then = (std::time_t)when, now = std::time(nullptr);
    std::tm           a{}, b{};
    localtime_r(&then, &a);
    localtime_r(&now, &b);
    const auto day = [](const std::tm& t) {
        return ch::sys_days{ch::year{t.tm_year + 1900} / ch::month{(unsigned)t.tm_mon + 1} / ch::day{(unsigned)t.tm_mday}};
    };
    const long days = (day(b) - day(a)).count();
    if (days == 0) return aurore::getStr("playshelf/game/today");
    if (days == 1) return aurore::getStr("playshelf/game/yesterday");
    if (days > 1 && days <= 30) return aurore::getStr("playshelf/game/days_ago", days);
    return date_text((uint32_t)((a.tm_year + 1900) * 10000 + (a.tm_mon + 1) * 100 + a.tm_mday));
}

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
    return std::format("https://ec.nintendo.com/apps/{}/{}", nsu_id, country);
}

// A row of a card of facts: its name, its value (nothing when it has none).
aurore::Box* chip_block(const std::string& name, const std::vector<Pill>& pills, const char* ink, const char* tint)
{
    auto* block = new aurore::Box(aurore::Axis::COLUMN);
    block->setAlignItems(aurore::AlignItems::STRETCH);
    block->setMarginTop(16);
    aurore::Label* caption = kit::label(name, 15, "cadran/text3");
    caption->setMarginBottom(8);
    block->addView(caption);
    auto* row = new aurore::Box(aurore::Axis::ROW);
    for (const Pill& p : pills)
        row->addView(p.on ? chip(p.text, "language", "cadran/teal", "cadran/teal_soft") : chip(p.text, nullptr, ink, tint));
    kit::wrap(row);
    block->addView(row);
    return block;
}

const char* score_ink(int score)
{
    return score >= 75 ? "cadran/teal" : score >= 50 ? "cadran/amber" : "cadran/coral";
}

const char* score_tint(int score)
{
    return score >= 75 ? "cadran/teal_soft" : score >= 50 ? "cadran/amber_surf" : "cadran/coral_soft";
}

aurore::Box* score_tile(const std::string& value, const std::string& caption, const std::string& sub, const char* ink,
                        const char* tint)
{
    auto* tile = new aurore::Box(aurore::Axis::COLUMN);
    tile->setWidth(240);
    tile->setPaddingTop(14);
    tile->setPaddingBottom(14);
    tile->setPaddingLeft(18);
    tile->setPaddingRight(18);
    tile->setCornerRadius(14);
    tile->setThemeColor("backgroundColor", tint);
    tile->setMarginRight(12);
    tile->setMarginBottom(12);
    tile->addView(kit::display(value, 30, ink));
    aurore::Label* name = kit::label(caption, 15, "cadran/text2");
    name->setMarginTop(2);
    tile->addView(name);
    if (!sub.empty()) tile->addView(kit::label(sub, 14, "cadran/text3"));
    return tile;
}

aurore::Box* size_bar(const std::vector<Part>& parts, uint64_t free_bytes, const std::string& caption, const char* caption_ink)
{
    uint64_t need = 0;
    for (const Part& p : parts) need += p.bytes;
    auto* block = new aurore::Box(aurore::Axis::COLUMN);
    block->setAlignItems(aurore::AlignItems::STRETCH);
    block->setMarginTop(16);
    // The track: each part as long as its bytes, the free space left over in the well's tint.
    auto* track = new aurore::Box(aurore::Axis::ROW);
    track->setHeight(12);
    track->setCornerRadius(6);
    track->setThemeColor("backgroundColor", "cadran/well");
    for (const Part& p : parts) {
        if (!p.bytes) continue;
        auto* seg = new aurore::Box(aurore::Axis::ROW);
        seg->setHeight(12);
        seg->setGrow((float)p.bytes);
        seg->setShrink(1);
        seg->setThemeColor("backgroundColor", p.ink);
        track->addView(seg);
    }
    if (free_bytes > need) {
        auto* rest = new aurore::Box(aurore::Axis::ROW);
        rest->setHeight(12);
        rest->setGrow((float)(free_bytes - need));
        rest->setShrink(1);
        track->addView(rest);
    }
    block->addView(track);
    // The legend: a dot and a name for each part.
    auto* legend = new aurore::Box(aurore::Axis::ROW);
    legend->setMarginTop(10);
    for (const Part& p : parts) {
        if (!p.bytes) continue;
        auto* item = new aurore::Box(aurore::Axis::ROW);
        item->setAlignItems(aurore::AlignItems::CENTER);
        item->setMarginRight(18);
        item->setMarginBottom(4);
        auto* dot = new aurore::Box(aurore::Axis::ROW);
        dot->setWidth(10);
        dot->setHeight(10);
        dot->setCornerRadius(3);
        dot->setThemeColor("backgroundColor", p.ink);
        dot->setMarginRight(7);
        item->addView(dot);
        item->addView(kit::label(p.label, 14, "cadran/text2"));
        legend->addView(item);
    }
    kit::wrap(legend);
    block->addView(legend);
    aurore::Label* line = kit::label(caption, 15, caption_ink);
    line->setMarginTop(4);
    block->addView(line);
    return block;
}

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

ShotActivity::~ShotActivity() { *this->alive = false; }

aurore::View* ShotActivity::createContentView()
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

void ShotActivity::load()
{
    this->counter->setText(std::format("{} / {}", this->at, this->count));
    const int wanted = this->at;
    ui::Weak  weak(this->alive);
    library_icons::picture(titledb::nlib_screen_url(this->base, wanted), FULL_W, FULL_H,
                           [this, weak, wanted](const library_icons::Picture& p) {
                               const ui::Alive held = weak.lock();
                               if (held && *held && wanted == this->at) library_icons::show(this->image, p);
                           });
}

}   // namespace game_parts
