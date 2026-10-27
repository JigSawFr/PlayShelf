// BannerCard — a game as a wide card (the Catalogue's and the Library's
// featured rows): its banner filling the card, fading into the page at the
// bottom, its name and a line under it laid over it, a small chip above them
// (what makes it featured: new, an update to get). A on it runs its action.
// Until its picture comes, a soft gradient of the page's surfaces. Then
// BannerBackdrop: a banner behind a card's right part.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <cstdint>
#include <string>

class BannerCard : public brls::Box
{
  public:
    BannerCard(float width, float height);
    ~BannerCard() override;

    void set_title(const std::string& text);
    void set_line(const std::string& text);
    // The chip above the name: its text and its ink ("cadran/teal"…); empty, none.
    void set_kicker(const std::string& text, const char* ink);
    // Its picture, `w` × `h` RGBA pixels, drawn to fill the card (cropped):
    // a texture of its own, freed with it.
    void set_picture(const uint8_t* rgba, int w, int h);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    brls::Label* title = nullptr;
    brls::Label* line = nullptr;
    brls::Box*   kicker = nullptr;
    brls::Label* kicker_text = nullptr;
    int          texture = 0;
    int          picture_w = 0, picture_h = 0;
    brls::Time   since = 0;   // the picture arrived then (µs): it fades in; 0 at rest
};

// A game's banner behind a card's right part (a download's): cropped to
// fill its bounds, fading in from the card's own surface on its left, dimmed
// for the words in front of it. Placed absolutely, first in its card.
class BannerBackdrop : public brls::View
{
  public:
    BannerBackdrop();
    ~BannerBackdrop() override;
    // As BannerCard's.
    void set_picture(const uint8_t* rgba, int w, int h);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    int        texture = 0;
    int        picture_w = 0, picture_h = 0;
    brls::Time since = 0;
};
