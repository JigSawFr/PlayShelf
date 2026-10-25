// CadranDigits — a line of text in the display face of the Cadran boards
// (Sora Bold, OFL, resources/font/sora_bold_latin.ttf, loaded as
// "cadran/digits"; the default font when it is missing): the big figures,
// the card values, the chips. borealis' Label draws with the one default font.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

class CadranDigits : public brls::View
{
  public:
    CadranDigits();

    void setText(const std::string& text);
    void setFontSize(float size);
    void setTextColor(NVGcolor color);
    // At most that wide: a longer text is drawn smaller to fit (0: never).
    void setFitWidth(float width);
    // At most as wide as its parent is laid out: a longer text is drawn
    // smaller to fit, the room beside it whatever the language (fitWidth
    // then follows the parent).
    void setFitParent(bool fit);
    // The figures at the font size, the words between them (min, h, a unit in
    // any language) at half of it, on the same baseline: "40 min", "1 h 20"
    // as a dial writes them. A text with no figure keeps one size.
    void setSmallUnits(bool small);
    // Drawn Preferences › Text size's times larger, as the labels are
    // (ui/text_size); off by default, the figures sized for their place.
    void setFollowsTextSize(bool follows);
    // A new text rolls in from below while the old one leaves upwards (a
    // counter's wheel), once a text was shown; off by default. Nothing with
    // the motion off (ui/effects).
    void setRolls(bool rolls);
    // The size taken again after the text size changed (ui/text_size).
    void remeasure() { this->measure(); }

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

    static brls::View* create();

  private:
    std::string text;
    float fontSize = 32.0f;
    float drawSize = 32.0f;   // fontSize, or less to fit fitWidth
    float fitWidth = 0.0f;
    bool fitParent = false;
    bool smallUnits = false;
    bool followsTextSize = false;
    bool rolls = false;
    // The text before, leaving (rolls): until when (µs), and its size.
    std::string leaving;
    brls::Time  roll_since = 0;
    float       leaving_size = 32.0f;
    bool        drawn = false;   // shown at least once: a change may roll
    NVGcolor color;
    int font;

    void measure();   // the view's size from the text
    struct Run
    {
        std::string text;
        bool figure;   // drawn at the full size
    };
    std::vector<Run> runs(const std::string& text) const;   // the text cut into figures and words
    float advance(NVGcontext* vg, float size) const;         // its width at `size`
    // `text` written at `size` within (x, y, width, height), at its middle.
    void write(NVGcontext* vg, const std::string& text, float size, float x, float y, float height) const;
};
