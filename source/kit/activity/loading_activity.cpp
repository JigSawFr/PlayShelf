// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/loading_activity.hpp"

using namespace brls::literals;

void LoadingActivity::onContentAvailable()
{
    // The light theme: its cool white, the wordmark and the bar in its inks,
    // so the screen does not flash navy before the first one (the icon keeps
    // its tile). Else the brand's navy, as the store banner.
    const bool light = brls::Application::getPlatform()->getThemeVariant() == brls::ThemeVariant::LIGHT;
    this->bar->setBrand(!light);
    this->dial->setLight(light);
    if (light) {
        brls::Theme theme = brls::Application::getTheme();
        this->root->setBackgroundColor(theme["brls/background"]);
        this->anchor->setColor(theme["brls/background"]);
        this->wordmark->setImageFromRes("img/wordmark-light.png");
        this->tagline->setTextColor(theme["cadran/text2"]);
        this->step_text->setTextColor(theme["cadran/text2"]);
    }
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

bool LoadingActivity::inIntro() const
{
    return this->introStart && !this->skipped && brls::getCPUTimeUsec() - this->introStart < INTRO_US;
}
