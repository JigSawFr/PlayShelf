// Host tests for the boards' palettes (ui/cadran_palette.hpp): every pair
// they list in CONTRAST, in both themes: an ink at 4.5:1 or more on each
// surface it is drawn on, a translucent one blended first over the
// background (WCAG 1.4.3); the focus ring at 3:1 or more on the background
// (WCAG 1.4.11). The same pairs are checked at run time for a theme file.
#include "check.h"
#include <cmath>
#include <cstdio>
#include <cstring>

#include <aurore/cadran/ui/cadran_palette.hpp>

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

// A key's colour in `p`, through the palettes' KEYS.
bool lookup(const Palette& p, const char* key, Rgba* out)
{
    for (const cadran::palette::Key& k : cadran::palette::KEYS)
        if (std::strcmp(k.name, key) == 0) {
            *out = p.*k.color;
            return true;
        }
    return false;
}

// Every pair the palettes promise (cadran::palette::CONTRAST, the ones
// Theme::checkContrast looks at when a theme file changes them).
void check(const char* theme, const Palette& p)
{
    for (const cadran::palette::Contrast& pair : cadran::palette::CONTRAST) {
        Rgba ink{}, surface{};
        CHECK(lookup(p, pair.ink, &ink) && lookup(p, pair.surface, &surface));
        char what[96];
        std::snprintf(what, sizeof(what), "%s on %s", pair.ink, pair.surface);
        CHECK(at_least(theme, what, ink, surface, p, pair.minimum));
    }
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
