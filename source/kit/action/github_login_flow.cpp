// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/github_login_flow.hpp"

#include <atomic>
#include <aurore.hpp>
#include <chrono>
#include <memory>
#include <thread>

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/util/text.hpp>

#include "ui/ui.hpp"
#include "util/github_auth.hpp"

using namespace aurore::literals;

namespace github_login_flow
{

namespace
{
bool s_busy = false;   // one sign-in at a time

// The code, the address and its QR code, until GitHub answers or the
// developer cancels.
void show_code(const github_auth::DeviceCode& code, std::function<void()> done)
{
    auto cancelled = std::make_shared<std::atomic<bool>>(false);

    // The code to type in the display face, large, its address's QR code
    // beside it to open the page from a phone.
    auto* row = new aurore::Box(aurore::Axis::ROW);
    row->setAlignItems(aurore::AlignItems::CENTER);
    row->setJustifyContent(aurore::JustifyContent::CENTER);
    aurore::Label* user_code = cadran::kit::display(code.user_code, 44);
    user_code->setMarginRight(32);
    row->addView(user_code);
    row->addView(cadran::kit::qr(code.verification_uri, 200, "", "playshelf/github/code_note"_i18n, 260));
    auto* dialog = ui::dialog(aurore::getStr("playshelf/github/code_body", text::short_url(code.verification_uri)), row);
    ui::add_cancel(dialog, [cancelled]() { *cancelled = true; });
    dialog->open();

    // Polls for up to 15 minutes: on its own thread, not holding up the play
    // log, the game icons and the rest of aurore::async's single queue.
    ui::in_background("GitHub sign-in", [code, cancelled, dialog, done]() {
        int interval = code.interval;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(code.expires_in);
        github_auth::Poll result = github_auth::Poll::Expired;
        std::string token, error;
        while (!*cancelled && !ui::quitting() && std::chrono::steady_clock::now() < deadline) {
            for (int s = 0; s < interval * 10 && !*cancelled && !ui::quitting(); s++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (*cancelled || ui::quitting()) break;
            result = github_auth::poll(code, &token, &error);
            if (result == github_auth::Poll::SlowDown) interval += 5;
            else if (result != github_auth::Poll::Pending) break;
        }
        aurore::sync([result, token, error, cancelled, dialog, done]() {
            s_busy = false;
            if (*cancelled) return;   // the dialog is already closed
            *cancelled = true;
            dialog->close();
            std::string err = error;
            switch (result) {
                case github_auth::Poll::Token:
                    if (github_auth::save_token(token, &err)) {
                        ui::notify("playshelf/github/signed_in"_i18n);
                        if (done) done();
                    } else {
                        ui::error(aurore::getStr("playshelf/github/failed", err));
                    }
                    break;
                case github_auth::Poll::Denied:
                    ui::info("playshelf/github/denied"_i18n);
                    break;
                case github_auth::Poll::Failed:
                    ui::error(aurore::getStr("playshelf/github/failed", err));
                    break;
                default:   // expired (or still pending at the deadline)
                    ui::error("playshelf/github/expired"_i18n);
                    break;
            }
        });
    });
}
}   // namespace

void sign_in(std::function<void()> done)
{
    if (s_busy) return;
    s_busy = true;
    ui::notify("playshelf/github/starting"_i18n);
    ui::in_background("GitHub sign-in", [done]() {
        github_auth::DeviceCode code;
        std::string err;
        const bool ok = github_auth::start(&code, &err);
        aurore::sync([ok, code, err, done]() {
            if (!ok) {
                s_busy = false;
                ui::error(aurore::getStr("playshelf/github/failed", err));
                return;
            }
            show_code(code, done);   // s_busy until it ends
        });
    });
}

void open(std::function<void()> done)
{
    if (github_auth::token().empty()) {
        sign_in(done);
        return;
    }
    ui::confirm("playshelf/github/sign_out_body"_i18n, "playshelf/github/sign_out"_i18n, [done]() {
        github_auth::forget_token();
        ui::notify("playshelf/github/signed_out"_i18n);
        if (done) done();
    }, nullptr, true);
}

}   // namespace github_login_flow
