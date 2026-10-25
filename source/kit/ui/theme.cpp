// ui — the palette's colours by role, and the console's system font (see ui.hpp).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "util/text.hpp"

namespace ui
{

#ifdef __SWITCH__
namespace
{
// The default font's name registered again with the system's `type` of
// shared font (its data is the system's, not copied), then the fallbacks the
// Chinese font had, and `also` (fontstash looks one level deep). False when
// the system does not give it.
bool register_default(PlSharedFontType type, int also)
{
    PlFontData data;
    if (R_FAILED(plGetSharedFontByType(&data, type))) return false;
    if (!brls::Application::loadFontFromMemory(brls::FONT_CHINESE_SIMPLIFIED, data.address, data.size, false))
        return false;
    const int font = brls::Application::getFont(brls::FONT_CHINESE_SIMPLIFIED);
    NVGcontext* vg = brls::Application::getNVGContext();
    for (const std::string& other : { brls::FONT_REGULAR, brls::FONT_CHINESE_SIMPLIFIED_EXT, brls::FONT_CHINESE_TRADITIONAL,
                                      brls::FONT_KOREAN_REGULAR, brls::FONT_SWITCH_ICONS, brls::FONT_MATERIAL_ICONS,
                                      brls::FONT_EMOJI }) {
        const int id = brls::Application::getFont(other);
        if (id != brls::FONT_INVALID && id != font) nvgAddFallbackFontId(vg, font, id);
    }
    if (also != brls::FONT_INVALID) nvgAddFallbackFontId(vg, font, also);
    return true;
}
}   // namespace
#endif

void use_font_for(const std::string& locale)
{
#ifdef __SWITCH__
    // borealis (wiliwili's branch) draws every label with the Simplified
    // Chinese system font, the Latin one only filling its gaps. That font has
    // the pinyin letters é, è, ê… as full-width glyphs ("prot é g é e"), and a
    // full-width "·" and "…". Chinese keeps it: the han in its own style. The
    // default font is looked up for each new label (Aurore, patch 0013).
    static const int chinese = brls::Application::getFont(brls::FONT_CHINESE_SIMPLIFIED);   // as loaded
    static bool latin = false;   // what the name holds now (the Chinese font at start)
    const bool want_latin = !text::starts_with(locale, "zh");
    if (want_latin == latin) return;
    if (register_default(want_latin ? PlSharedFontType_Standard : PlSharedFontType_ChineseSimplified,
                         want_latin ? chinese : brls::FONT_INVALID))
        latin = want_latin;
#else
    (void)locale;
#endif
}

}   // namespace ui
