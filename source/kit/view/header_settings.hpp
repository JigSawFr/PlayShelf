// HeaderSetting — the language and the theme in the main screen's header,
// left of the notification bell: the language's code ("FR", "EN", the
// console's own one for "system") in a rounded frame, and the theme as a
// sun (light), a moon (dark) or a half-filled disc (the console's); the
// text size after them, as "AA" (a small A and
// a large one, the large one teal while the text is larger). A code
// rather than a flag: a flag names a country, not a language, and fr-CA,
// es-419 or zh-Hant have none that fits. A tap opens the same list as
// Preferences (ui::pick_language / pick_theme / pick_text_size); not
// focusable, like the bell: − opens them from any tab (activity/main_activity),
// as the footer says (and Ⓨ the bell's list).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class HeaderSetting : public brls::View
{
  public:
    enum class Kind { LANGUAGE, THEME, TEXT_SIZE };

    explicit HeaderSetting(Kind kind);

    // Puts them in the main screen's header, when it first takes the focus
    // (after NoticeBell::install's bell). Called once, at start-up.
    static void install();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;
    // Touched although it is not focusable.
    brls::View* hitTest(brls::Point point) override;

  private:
    Kind kind;

    void draw_language(NVGcontext* vg, float x, float y, float width, float height);
    void draw_theme(NVGcontext* vg, float x, float y, float width, float height);
    void draw_text_size(NVGcontext* vg, float x, float y, float width, float height);
};
