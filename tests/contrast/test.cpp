// Host tests for the boards' palettes (ui/cadran_palette.hpp): every ink at
// 4.5:1 or more on each surface it is drawn on, a translucent one blended
// first over the background, in both themes (WCAG 1.4.3); the focus ring
// and its glow at 3:1 or more on the background (WCAG 1.4.11).
#include "check.h"
#include <cmath>
#include <cstdio>

#include "ui/cadran_palette.hpp"

using cadran::palette::Palette;
using cadran::palette::Rgba;

namespace
{
double channel(uint8_t c)
{
    const double v = c / 255.0;
    return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}

double luminance(Rgba c)
{
    return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
}

// `top` drawn over the opaque `under`.
Rgba over(Rgba top, Rgba under)
{
    const double a = top.a / 255.0;
    const auto mix = [a](uint8_t t, uint8_t u) { return (uint8_t)std::lround(t * a + u * (1 - a)); };
    return { mix(top.r, under.r), mix(top.g, under.g), mix(top.b, under.b), 255 };
}

double ratio(Rgba a, Rgba b)
{
    const double la = luminance(a), lb = luminance(b);
    return (std::fmax(la, lb) + 0.05) / (std::fmin(la, lb) + 0.05);
}

bool at_least(const char* theme, const char* what, Rgba ink, Rgba surface, const Palette& p, double min)
{
    const double r = ratio(over(ink, over(surface, p.bg)), over(surface, p.bg));
    if (r >= min) return true;
    std::printf("%s: %s %.2f:1, under %.1f:1\n", theme, what, r, min);
    return false;
}

void check(const char* theme, const Palette& p)
{
    // The text and the inks, on the background, a card and the second surface.
    const struct { const char* name; Rgba ink; } inks[] = {
        { "text", p.text },   { "text2", p.text2 }, { "text3", p.text3 }, { "teal", p.teal },
        { "amber", p.amber }, { "coral", p.coral }, { "blue", p.blue },   { "violet", p.violet },
    };
    const struct { const char* name; Rgba surface; } surfaces[] = { { "bg", p.bg }, { "surf", p.surf }, { "surf2", p.surf2 } };
    for (const auto& i : inks)
        for (const auto& s : surfaces) {
            char what[48];
            std::snprintf(what, sizeof(what), "%s on %s", i.name, s.name);
            CHECK(at_least(theme, what, i.ink, s.surface, p, 4.5));
        }
    // Where the palettes put an ink on a fill of their own.
    CHECK(at_least(theme, "amber on amber_surf", p.amber, p.amber_surf, p, 4.5));
    CHECK(at_least(theme, "pill_ok_ink on pill_ok", p.pill_ok_ink, p.pill_ok, p, 4.5));
    CHECK(at_least(theme, "teal on rail_active", p.teal, p.rail_active, p, 4.5));
    CHECK(at_least(theme, "key_fg on key_bg", p.key_fg, p.key_bg, p, 4.5));
    CHECK(at_least(theme, "navy on teal_fill", cadran::palette::NAVY, p.teal_fill, p, 4.5));   // a confirmation's label
    CHECK(at_least(theme, "navy on coral_fill", cadran::palette::NAVY, p.coral_fill, p, 4.5));
    // The focus ring, both ends of its pulse.
    CHECK(at_least(theme, "hl1 on bg", p.hl1, p.bg, p, 3.0));
    CHECK(at_least(theme, "hl2 on bg", p.hl2, p.bg, p, 3.0));
}
}   // namespace

int main()
{
    // The arithmetic first: black on white is 21:1, a colour on itself 1:1.
    CHECK(std::fabs(ratio({ 0, 0, 0, 255 }, { 255, 255, 255, 255 }) - 21.0) < 0.01);
    CHECK(std::fabs(ratio({ 0x12, 0x34, 0x56, 255 }, { 0x12, 0x34, 0x56, 255 }) - 1.0) < 1e-9);
    check("dark", cadran::palette::DARK);
    check("light", cadran::palette::LIGHT);
    return CHECK_DONE("palette contrast assertions passed");
}
