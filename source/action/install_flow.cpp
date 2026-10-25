// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/install_flow.hpp"

#include <algorithm>
#include <atomic>
#include <borealis.hpp>
#include <memory>

#include <fmt/format.h>

#include "install/console_core.hpp"
#include "install/http_source.hpp"
#include "install/installer.hpp"
#include "ui/cadran_kit.hpp"
#include "ui/cadran_modal.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

namespace install_flow
{
namespace
{
std::string s_last_url = "https://";

std::string mb_text(uint64_t bytes)
{
    std::string text = fmt::format("{:.1f}", (double)bytes / (1024.0 * 1024.0));
    const size_t dot = text.find('.');
    if (dot != std::string::npos) text.replace(dot, 1, "playshelf/games/decimal_mark"_i18n);
    return text;
}

// The dialog of an install: what is under way, the package, a bar and
// "3,2 of 4,7 MB", and Cancel (B too), updated from the install's thread
// at most once a percent (through brls::sync). Cancel sets the flag the
// install checks between pieces and closes the dialog at once; nothing
// touches it after it closed.
class Progress
{
  public:
    explicit Progress(const std::string& name)
    {
        this->body = cadran::progress_body("playshelf/install/preparing"_i18n, name);
        this->button = new brls::Button();
        this->button->setText("hints/cancel"_i18n);
        cadran::style_button(this->button);
        this->button->setWidth(260);
        this->button->setMarginTop(28);
        this->button->setAlignSelf(brls::AlignSelf::CENTER);
        this->button->registerClickAction([this](brls::View*) {
            this->cancel();
            return true;
        });
        this->body.box->addView(this->button);
        this->dialog = new brls::Dialog(this->body.box);
        this->dialog->setCancelable(false);
        this->dialog->getAppletFrame()->registerAction(
            "hints/cancel"_i18n, brls::BUTTON_B, [this](brls::View*) {
                this->cancel();
                return true;
            },
            false, false, brls::SOUND_BACK);
        this->body.show(0.0f, "");
        this->dialog->open();
    }

    // From the install's thread.
    void written(uint64_t done, uint64_t total)
    {
        const int now = total ? (int)std::min<uint64_t>(100, done * 100 / total) : 0;
        if (now == this->percent.exchange(now)) return;
        brls::sync([this, done, total]() { this->show(done, total); });
    }

    const std::atomic<bool>* cancel_flag() const { return &this->cancelled; }
    bool                     was_cancelled() const { return this->cancelled.load(); }

    void close()
    {
        if (this->dialog) this->dialog->close();
        this->dialog = nullptr;
    }

  private:
    std::atomic<int>  percent{-1};
    std::atomic<bool> cancelled{false};
    brls::Dialog*     dialog = nullptr;
    brls::Button*     button = nullptr;
    cadran::Progress  body;

    void cancel()
    {
        if (!this->dialog) return;
        this->cancelled = true;
        this->close();
    }

    void show(uint64_t done, uint64_t total)
    {
        if (!this->dialog) return;
        this->body.heading->setText("playshelf/install/installing"_i18n);
        const float part = total ? std::min(1.0f, (float)done / (float)total) : 1.0f;
        this->body.show(part, brls::getStr("playshelf/dev_build/downloaded_mb", mb_text(done), mb_text(total)));
    }
};

// The package's name, from its address: what follows the last "/".
std::string name_of(const std::string& url)
{
    const size_t slash = url.find_last_of('/');
    std::string  name = slash == std::string::npos ? url : url.substr(slash + 1);
    const size_t query = name.find('?');
    if (query != std::string::npos) name.resize(query);
    return name.empty() ? url : name;
}

struct Outcome
{
    bool        ok = false;
    bool        unsigned_content = false;   // stopped by SignaturePolicy::Require
    std::string why;
    std::string titles;    // "0100…0000, 0100…0800"
    uint64_t    written = 0;
};

Outcome install(const std::string& url, install::SignaturePolicy policy, Progress& progress)
{
    using namespace install;
    Outcome    out;
    HttpSource src(url, {}, progress.cancel_flag());
    if (!src.open(&out.why)) return out;
    const auto pkg = package_open(src, &out.why);
    if (!pkg) return out;
    CoreConsole console(CoreStorage_Sd);
    if (!console.ok()) {
        out.why = console.error();
        return out;
    }
    const auto keys = core_keys(&out.why);
    if (!keys) return out;
    InstallCallbacks cb;
    cb.cancel = progress.cancel_flag();
    cb.progress = [&progress](uint64_t done, uint64_t total, const std::string&) { progress.written(done, total); };
    Installer inst(src, *pkg, console, *keys, InstallOptions{policy, false}, cb);
    if (!inst.prepare(&out.why)) return out;
    for (const TitlePlan& t : inst.plan())
        out.titles += (out.titles.empty() ? "" : ", ") + fmt::format("{:016X}", t.meta.title_id);
    progress.written(0, inst.bytes_to_write());
    out.ok = inst.run(&out.why);
    out.unsigned_content = !out.ok && out.why.find("not signed by Nintendo") != std::string::npos;
    out.written = inst.bytes_to_write();
    return out;
}

void run(const std::string& url, install::SignaturePolicy policy)
{
    const std::string name = name_of(url);
    auto              progress = std::make_shared<Progress>(name);
    ui::in_background("install", [url, name, policy, progress]() {
        const Outcome o = install(url, policy, *progress);
        brls::sync([url, name, o, progress]() {
            const bool cancelled = progress->was_cancelled();
            progress->close();
            if (o.ok) {
                brls::Logger::info("install: {} installed ({} bytes)", name, o.written);
                ui::info(brls::getStr("playshelf/install/done", name, o.titles, mb_text(o.written)));
            } else if (cancelled) {
                ui::notify("playshelf/install/cancelled"_i18n);
            } else if (o.unsigned_content) {
                ui::confirm(brls::getStr("playshelf/install/unsigned_body", o.why), "playshelf/install/unsigned_confirm"_i18n,
                            [url]() { run(url, install::SignaturePolicy::Off); });
            } else {
                brls::Logger::warning("install: {} failed: {}", name, o.why);
                ui::error(brls::getStr("playshelf/install/failed", name), o.why);
            }
        });
    });
}
}   // namespace

void from_url()
{
    if (ui::refuse_read_only()) return;
    ui::prompt_text("playshelf/install/url_header"_i18n, s_last_url, 1024, [](const std::string& url) {
        if (url.rfind("https://", 0) != 0 || url.size() <= 8) {
            ui::error("playshelf/install/https_only"_i18n);
            return;
        }
        s_last_url = url;
        run(url, install::SignaturePolicy::Require);
    });
}

}   // namespace install_flow
