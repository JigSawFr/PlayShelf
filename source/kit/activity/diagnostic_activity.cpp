// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/diagnostic_activity.hpp"

#include <sstream>

#include "action/upload_flow.hpp"
#include "ui/ui.hpp"
#include "util/diagnostics.hpp"

using namespace aurore::literals;

namespace
{
// Lines per focusable chunk: small enough to stay on screen whole.
constexpr int CHUNK_LINES = 12;
}

void DiagnosticActivity::onContentAvailable()
{
    this->report = diagnostic::current_report();

    std::istringstream in(this->report);
    std::string line, chunk;
    int lines = 0;
    auto flush = [this, &chunk, &lines]() {
        if (chunk.empty()) return;
        if (chunk.back() == '\n') chunk.pop_back();
        auto* label = new aurore::Label();
        label->setText(chunk);
        label->setFontSize(18);
        label->setSingleLine(false);
        label->setFocusable(true);
        label->setMarginBottom(8);
        this->list->addView(label);
        chunk.clear();
        lines = 0;
    };
    while (std::getline(in, line)) {
        chunk += line + "\n";
        if (++lines >= CHUNK_LINES) flush();
    }
    flush();

    this->getContentView()->registerAction("playshelf/dev/report_save"_i18n, aurore::BUTTON_Y, [this](aurore::View*) {
        std::string err;
        const std::string path = diagnostic::save(this->report, &err);
        if (path.empty()) ui::error("playshelf/toast/diag_err"_i18n, err);
        else ui::notify(aurore::getStr("playshelf/toast/diag_saved", path));
        return true;
    });
    this->getContentView()->registerAction("playshelf/dev/report_upload"_i18n, aurore::BUTTON_X, [this](aurore::View*) {
        upload_flow::send_report(this->report);
        return true;
    });
}
