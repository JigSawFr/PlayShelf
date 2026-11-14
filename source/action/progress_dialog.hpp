// progress_dialog — a dialog with a bar and Cancel, for work done on a worker thread (compressing a
// game, checking one): `show` from the UI thread (aurore::sync), `close` when the work ends. Cancel
// sets the flag the worker watches and closes the dialog.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <format>
#include <string>

#include <aurore.hpp>
#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/ui/cadran_modal.hpp>

namespace progress_dialog
{

using namespace aurore::literals;

inline std::string mb_text(uint64_t bytes)
{
    return std::format("{:.1f}", (double)bytes / (1024.0 * 1024.0));
}

// A dialog with a bar and Cancel, updated from the worker's thread at most once a percent.
class Progress
{
  public:
    Progress(const std::string& heading, const std::string& name, std::atomic<bool>* cancel)
        : cancel_flag(cancel)
    {
        this->body = cadran::progress_body(heading, name);
        this->button = new aurore::Button();
        this->button->setText("hints/cancel"_i18n);
        cadran::style_button(this->button);
        this->button->setWidth(260);
        this->button->setMarginTop(28);
        this->button->setAlignSelf(aurore::AlignSelf::CENTER);
        this->button->registerClickAction([this](aurore::View*) {
            this->cancel();
            return true;
        });
        this->body.box->addView(this->button);
        this->dialog = new aurore::Dialog(this->body.box);
        this->dialog->setCancelable(false);
        this->dialog->getAppletFrame()->registerAction(
            "hints/cancel"_i18n, aurore::BUTTON_B, [this](aurore::View*) {
                this->cancel();
                return true;
            },
            false, false, aurore::SOUND_BACK);
        this->body.show(0.0f, "");
        this->dialog->open();
    }

    void show(const std::string& heading, uint64_t done, uint64_t total)
    {
        if (!this->dialog) return;
        this->body.heading->setText(heading);
        const float part = total ? std::min(1.0f, (float)done / (float)total) : 1.0f;
        this->body.show(part, std::format("{} / {} MB", mb_text(done), mb_text(total)));
    }

    void close()
    {
        if (this->dialog) this->dialog->close();
        this->dialog = nullptr;
    }

  private:
    void cancel()
    {
        if (!this->dialog) return;
        if (this->cancel_flag) *this->cancel_flag = true;
        this->close();
    }

    std::atomic<bool>* cancel_flag;
    aurore::Dialog*    dialog = nullptr;
    aurore::Button*    button = nullptr;
    cadran::Progress   body;
};

}   // namespace progress_dialog
