// GameTile — a game of the library as a cover in a grid (tab/library_tab):
// its icon (a gamepad on the well until one arrives, or for a game without
// one), its name on one line and a value under it (its size, what the server
// has of it). From PlayGuard's.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>

class CadranIcon;

class GameTile : public brls::Box
{
  public:
    // Its width, margins included: the grid fits as many a row as its width
    // takes.
    static constexpr float WIDTH = 176;

    GameTile();

    void set_name(const std::string& text);
    void set_detail(const std::string& text);
    void set_detail_color(NVGcolor color);
    // An NVG image in place of the gamepad; `owned`, this tile frees it.
    void set_icon_texture(int texture, bool owned);
    bool has_icon() const { return this->iconed; }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    brls::Box*   cover = nullptr;
    CadranIcon*  placeholder = nullptr;
    brls::Image* icon = nullptr;
    brls::Label* name = nullptr;
    brls::Label* detail = nullptr;
    bool         iconed = false;
    brls::Time   icon_since = 0;   // the icon arrived then (µs): it fades in; 0 at rest
};
