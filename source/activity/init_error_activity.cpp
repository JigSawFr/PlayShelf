// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "activity/init_error_activity.hpp"

#include <fmt/format.h>

#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

void InitErrorActivity::onContentAvailable()
{
    // No Help & diagnostics here to send a report from.
    this->error_code->setText(ui::rc_text(app::init_result(), false));

    SysInfo si;
    sysinfo_get(&si);
    const std::string fw = ui::hos_text(si.hos_version);
    std::string ams = si.ams_valid ? fmt::format("{}.{}.{}", si.ams_major, si.ams_minor, si.ams_micro)
                                   : "playshelf/common/unavailable"_i18n;
    this->error_fw->setText(brls::getStr("playshelf/init_error/firmware", fw, ams, ui::storage_short(si)));

    this->getContentView()->registerAction("hints/exit"_i18n, brls::BUTTON_B, [](brls::View*) {
        brls::Application::quit();
        return true;
    });
}
