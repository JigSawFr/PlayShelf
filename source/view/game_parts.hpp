// The parts a game's sheet is made of (GameActivity): the banner, the chips
// and cards, the facts' rows, the full-screen screenshot, and the few words
// they are given (language, date, eShop address).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <aurore.hpp>
#include <aurore/cadran/ui/guarded.hpp>
#include <aurore/cadran/ui/ui.hpp>

namespace game_parts
{

constexpr int SHOT_W = 320, SHOT_H = 180;   // a screenshot's tile

// The banner: its picture, its lower part fading into the page for the
// head laid over it to read on any picture.
class Banner : public aurore::Image
{
  public:
    Banner();
    void draw(NVGcontext* vg, float x, float y, float width, float height, aurore::Style style,
              aurore::FrameContext* ctx) override;
};

// A chip: its mark and its text, on a tint of its ink.
aurore::Box* chip(const std::string& text, const char* icon_name, const char* ink, const char* tint);

// A caption over a row of chips (languages, age ratings); the `on` ones
// stand out.
struct Pill
{
    std::string text;
    bool        on = false;
};
aurore::Box* chip_block(const std::string& name, const std::vector<Pill>& pills, const char* ink, const char* tint);

// How good a score (0-100) is, as the ink and the tint of its chip or tile:
// teal from 75, amber from 50, coral below.
const char* score_ink(int score);
const char* score_tint(int score);

// A tile of a score: its value big in the display face, its caption, and a
// line under it (how many gave it, nothing when none).
aurore::Box* score_tile(const std::string& value, const std::string& caption, const std::string& sub, const char* ink,
                        const char* tint);

// The space a game takes: a bar of its parts side by side in a track as long
// as the free space (or the parts, when they are longer), a legend of the
// parts and the `caption` under it, in the `caption_ink`.
struct Part
{
    std::string label;
    uint64_t    bytes = 0;
    const char* ink = "cadran/teal";
};
aurore::Box* size_bar(const std::vector<Part>& parts, uint64_t free_bytes, const std::string& caption, const char* caption_ink);

// Where the console stands on a tab's items: a mark in a round of its tint,
// a line in the display face, a line under it.
aurore::Box* status_card(const std::string& title, const std::string& line, const char* icon_name, const char* ink,
                         const char* tint);

// A row of a card of facts: its name, its value (nothing when it has none:
// the row is not made, nullptr).
aurore::DetailCell* fact(aurore::Box* card, const std::string& name, const std::string& value);

// A game's language in the language PlayShelf speaks ("ko" -> "coréen"),
// Nlib API's code itself for one PlayShelf has no word for.
std::string language_name(const std::string& code);

// "3 March 2017", in the language's order.
std::string date_text(uint32_t ymd);

// "Today", "Yesterday", "3 days ago" for a moment (POSIX) within a month, its
// date beyond; "" when it has none.
std::string since_text(uint64_t when);

// Its eShop page: ec.nintendo.com sends a phone to the shop of the country
// PlayShelf's language is spoken in.
std::string eshop_url(uint64_t nsu_id);

// One screenshot on the whole screen, black around it: ◀ ▶ the others, B
// back to the page.
class ShotActivity : public aurore::Activity
{
  public:
    ShotActivity(uint64_t base_id, int shots, int first) : base(base_id), count(shots), at(first) {}
    ~ShotActivity() override;

    aurore::View* createContentView() override;

  private:
    uint64_t       base;
    int            count, at;
    aurore::Image* image = nullptr;
    aurore::Label* counter = nullptr;
    ui::Alive      alive = ui::make_alive();

    void load();
};

}   // namespace game_parts
