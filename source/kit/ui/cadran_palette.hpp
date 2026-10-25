// cadran_palette — the boards' two palettes as plain values (no view, no
// nanovg): cadran::install_theme puts them in borealis' themes, and a host
// suite checks the contrast of every ink on every surface it is drawn on
// (tests/contrast).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>

namespace cadran::palette
{
struct Rgba
{
    uint8_t r, g, b, a;
};

// Dark: the boards' navy. Light: the style board's cool white, the inks
// darkened to 4.5:1 on every surface they are drawn on, the rail's active
// pill included (teal #0A705F, amber #8A5200, coral #B71C1C); the quiet
// text too, on both (text3 4.5:1 on the second surface). The focus ring
// in the dark teal, its glow no lighter than the fill teal: 3:1 or more
// on the background at any point of its pulse (WCAG 1.4.11). The dark
// theme's hairline at 14 % white: the cards stay apart on a TV that
// crushes its blacks.
struct Palette
{
    Rgba bg, text, text2, text3, surf, surf2, line, teal, teal_fill, teal_soft, amber, amber_fill, coral, coral_fill,
        coral_line, track, glow, glow_strong, amber_surf, amber_line, pill_ok, pill_ok_ink, rail_active, key_bg,
        key_fg, hl1, hl2, border;
    Rgba blue, violet;   // a release note's alerts, a build's tags
};

constexpr Rgba NAVY = { 0x0B, 0x14, 0x24, 255 };

constexpr Palette DARK = {
    NAVY,   // bg
    { 0xEE, 0xF2, 0xF8, 255 },   // text
    { 0xA9, 0xB5, 0xC9, 255 },   // text2
    { 0x83, 0x92, 0xAA, 255 },   // text3
    { 0x12, 0x1E, 0x33, 255 },   // surf
    { 0x1A, 0x2A, 0x45, 255 },   // surf2
    { 255, 255, 255, 36 },   // line
    { 0x2E, 0xC4, 0xA6, 255 },   // teal
    { 0x2E, 0xC4, 0xA6, 255 },   // teal_fill
    { 0x2E, 0xC4, 0xA6, 26 },   // teal_soft
    { 0xFF, 0xB5, 0x47, 255 },   // amber
    { 0xFF, 0xB5, 0x47, 255 },   // amber_fill
    { 0xFF, 0x7A, 0x7A, 255 },   // coral
    { 0xFF, 0x7A, 0x7A, 255 },   // coral_fill
    { 0xFF, 0x7A, 0x7A, 140 },   // coral_line
    { 255, 255, 255, 23 },   // track
    { 0x2E, 0xC4, 0xA6, 31 },   // glow
    { 0x2E, 0xC4, 0xA6, 51 },   // glow_strong
    { 0xFF, 0xB5, 0x47, 26 },   // amber_surf
    { 0xFF, 0xB5, 0x47, 90 },   // amber_line
    { 0x2E, 0xC4, 0xA6, 41 },   // pill_ok
    { 0x2E, 0xC4, 0xA6, 255 },   // pill_ok_ink
    { 0x2E, 0xC4, 0xA6, 36 },   // rail_active
    { 0xEE, 0xF2, 0xF8, 255 },   // key_bg
    NAVY,   // key_fg
    { 0x2E, 0xC4, 0xA6, 255 },   // hl1
    { 0x9A, 0xF0, 0xDC, 255 },   // hl2
    { 255, 255, 255, 60 },   // border
    { 0x4F, 0xA3, 0xFF, 255 },   // blue
    { 0x9B, 0x8C, 0xF5, 255 }   // violet
};

constexpr Palette LIGHT = {
    { 0xED, 0xF0, 0xF4, 255 },   // bg
    NAVY,   // text
    { 0x3F, 0x4C, 0x63, 255 },   // text2
    { 0x5F, 0x6E, 0x88, 255 },   // text3
    { 0xFF, 0xFF, 0xFF, 255 },   // surf
    { 0xF3, 0xF5, 0xF8, 255 },   // surf2
    { 0x0B, 0x14, 0x24, 26 },   // line
    { 0x0A, 0x70, 0x5F, 255 },   // teal
    { 0x14, 0xA3, 0x8A, 255 },   // teal_fill
    { 0x14, 0xA3, 0x8A, 26 },   // teal_soft
    { 0x8A, 0x52, 0x00, 255 },   // amber
    { 0xE0, 0x93, 0x1B, 255 },   // amber_fill
    { 0xB7, 0x1C, 0x1C, 255 },   // coral
    { 0xE5, 0x48, 0x4D, 255 },   // coral_fill
    { 0xE3, 0x9A, 0x9A, 255 },   // coral_line
    { 0x0B, 0x14, 0x24, 26 },   // track
    { 0x2E, 0xC4, 0xA6, 41 },   // glow
    { 0x2E, 0xC4, 0xA6, 56 },   // glow_strong
    { 0xFF, 0xF3, 0xDF, 255 },   // amber_surf
    { 0xF3, 0xD5, 0x9C, 255 },   // amber_line
    { 0xD8, 0xF3, 0xEC, 255 },   // pill_ok
    { 0x0A, 0x6E, 0x5C, 255 },   // pill_ok_ink
    { 0x14, 0xA3, 0x8A, 36 },   // rail_active
    NAVY,   // key_bg
    { 0xFF, 0xFF, 0xFF, 255 },   // key_fg
    { 0x0A, 0x70, 0x5F, 255 },   // hl1
    { 0x0F, 0x8A, 0x74, 255 },   // hl2
    { 0x0B, 0x14, 0x24, 60 },   // border
    { 0x1F, 0x5F, 0xC4, 255 },   // blue
    { 0x5B, 0x47, 0xC9, 255 }   // violet
};
}   // namespace cadran::palette
