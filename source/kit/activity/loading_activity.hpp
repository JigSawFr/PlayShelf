// LoadingActivity — the loading screen: the first activity, drawn before the
// rest of the start-up (main.cpp), which takes a moment on the console (the
// service probe, the SD-card reads, the main screen's build) and left the
// screen black until then. main.cpp runs the start-up one step per frame and
// moves the progress bar along; the first real screen is then pushed over this
// one, which stays at the bottom of the stack, holding the focus the first
// screen hands back. In the theme's background and texts; the wordmark and
// the bar in the brand's colours on a dark one.
// The intro (Preferences › Show the intro): the start-up takes
// a fraction of a second, too short to see this screen, so it stays at least
// INTRO_US, its bar filling at a steady pace; Ⓐ or Ⓑ skip the rest. Nothing
// else to press here.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

#include <borealis/cadran/view/loading_bar.hpp>

#include "view/loading_dial.hpp"

class LoadingActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/loading.xml");

    enum class Mode
    {
        Start,     // the start-up
        Closing,   // the last screen, the bar full, until PlayShelf is gone
    };

    explicit LoadingActivity(Mode shown = Mode::Start) : mode(shown) {}
    ~LoadingActivity() override;

    // The start-up steps, in order.
    enum class Step
    {
        Console,    // probing the console (app::init)
        Settings,   // the first screen chosen
        Screen,     // building the first screen
    };

    static constexpr brls::Time INTRO_US = 2000000;

    void onContentAvailable() override;
    // `step` is running: its text under the bar, and the bar at its share
    // (during the intro, the bar keeps its pace).
    void show(Step step);
    // From now on, the intro: INTRO_US, unless skipped.
    void startIntro();
    // Still on the intro: started, not skipped, INTRO_US not over yet.
    bool inIntro() const;

  private:
    BRLS_BIND(LoadingBar, bar, "loading_bar");
    BRLS_BIND(LoadingDial, dial, "loading_dial");
    BRLS_BIND(brls::Label, step_text, "loading_step");
    BRLS_BIND(brls::Rectangle, anchor, "loading_anchor");
    BRLS_BIND(brls::Box, root, "loading_root");
    BRLS_BIND(brls::Image, wordmark, "loading_wordmark");
    BRLS_BIND(brls::Label, tagline, "loading_tagline");

    // The wordmark and the bar for the theme's background, again when it
    // changes.
    void follow_theme();

    Mode       mode;
    bool       light_wordmark = false;   // the wordmark shown is the light one (the XML's: the dark one)
    bool       themed = false;     // theme_changed holds a subscription
    brls::Event<brls::ThemeVariant>::Subscription theme_changed;
    brls::Time introStart = 0;   // 0: no intro
    bool       skipped    = false;
};
