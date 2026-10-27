// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/loading_activity.hpp"

using namespace brls::literals;

LoadingActivity::~LoadingActivity()
{
    if (this->themed) brls::Application::getThemeChangedEvent()->unsubscribe(this->theme_changed);
}

void LoadingActivity::onContentAvailable()
{
    this->follow_theme();
    this->theme_changed = brls::Application::getThemeChangedEvent()->subscribe([this](brls::ThemeVariant) {
        this->follow_theme();
    });
    this->themed = true;
    if (this->mode == Mode::Closing) {
        this->step_text->setText("playshelf/loading/step_closing"_i18n);
        this->bar->setProgress(1.0f);
        return;
    }
    this->show(Step::Console);
}

void LoadingActivity::show(Step step)
{
    // How far the bar is while the step runs: the first screen showing is the
    // end. Its build (Today's first reading) is the longest part.
    float share = 0.0f;
    switch (step) {
        case Step::Console:
            this->step_text->setText("playshelf/loading/step_console"_i18n);
            share = 0.15f;
            break;
        case Step::Settings:
            this->step_text->setText("playshelf/loading/step_settings"_i18n);
            share = 0.45f;
            break;
        case Step::Screen:
            this->step_text->setText("playshelf/loading/step_screen"_i18n);
            share = 0.8f;
            break;
    }
    if (!this->introStart) this->bar->setProgress(share);
}

void LoadingActivity::startIntro()
{
    this->introStart = brls::getCPUTimeUsec();
    this->bar->fillOver(1.0f, INTRO_US);
    // The anchor holds the focus while this screen is the top one.
    for (brls::ControllerButton button : { brls::BUTTON_A, brls::BUTTON_B })
        this->anchor->registerAction("", button, [this](brls::View*) {
            this->skipped = true;
            return true;
        }, true);
}

void LoadingActivity::follow_theme()
{
    // On a light background (the light theme's cool white), the wordmark and
    // the bar in its inks, so the screen does not flash navy before the
    // first one (the icon keeps its tile); else the brand's, as the store
    // banner. The background and the texts are the theme's (XML).
    const NVGcolor bg = brls::Application::getTheme()["brls/background"];
    const bool light = 0.2126f * bg.r + 0.7152f * bg.g + 0.0722f * bg.b > 0.5f;
    this->bar->setBrand(!light);
    if (this->light_wordmark == light) return;
    this->light_wordmark = light;
    this->wordmark->setImageFromRes(light ? "img/wordmark-light.png" : "img/wordmark.png");
}

bool LoadingActivity::inIntro() const
{
    return this->introStart && !this->skipped && brls::getCPUTimeUsec() - this->introStart < INTRO_US;
}
