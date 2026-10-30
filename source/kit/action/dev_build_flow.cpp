// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/dev_build_flow.hpp"

#include <algorithm>
#include <atomic>
#include <format>
#include <aurore.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <vector>

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/ui/cadran_modal.hpp>
#include <aurore/cadran/util/text.hpp>

#include "action/github_login_flow.hpp"
#include "action/rollback_flow.hpp"
#include "activity/dev_build_activity.hpp"
#include "activity/dev_builds_activity.hpp"
#include "app.hpp"
#include "core/platform.h"
#include "core/sim_env.h"
#include "ui/app_hooks.hpp"
#include "ui/ui.hpp"
#include "util/dev_builds.hpp"
#include "util/github_auth.hpp"
#include "util/launcher.hpp"
#include "util/migrations.hpp"
#include "util/paths.hpp"
#include "util/rollback.hpp"

using namespace aurore::literals;

namespace dev_build_flow
{

namespace
{
bool s_busy = false;   // one list or download at a time

// The last list fetched (dev_builds::Cache), read from the SD card on the
// first opening of this run.
dev_builds::Cache s_cache;
bool              s_cache_read = false;

std::string cache_file() { return paths::cache_dir() + "/dev_builds.json"; }

void read_cache()
{
    if (s_cache_read) return;
    s_cache_read = true;
    std::string text;
    if (paths::read_file(cache_file(), text) && !dev_builds::decode_cache(text, &s_cache)) s_cache = {};
}

void keep(const dev_builds::Cache& cache)
{
    s_cache = cache;
    std::string err;
    // Only saves time on the next start: a write that fails is just logged.
    if (!paths::ensure_dir(paths::cache_dir()) ||
        !paths::atomic_write(cache_file(), dev_builds::encode_cache(cache), &err))
        aurore::Logger::warning("dev builds: cannot keep the list ({})", err);
}

// The .nro to replace, from the SD card root: the running one. On desktop,
// where nothing runs from an SD card, the usual place on the simulated one.
std::string sd_path()
{
#ifdef __SWITCH__
    return app::self_path();
#else
    return "/switch/playshelf/playshelf.nro";
#endif
}

// That file for the C library ("/switch/…" on the console, under
// ./playshelf_data/sd on desktop).
std::string file_path(const std::string& sd)
{
    const std::string root = paths::sd_root();
    return root == "/" ? sd : root + sd;
}

// "4,7" (MB, one decimal, the language's decimal mark).
std::string mb_text(uint64_t bytes)
{
    std::string text = std::format("{:.1f}", (double)bytes / (1024.0 * 1024.0));
    const size_t dot = text.find('.');
    if (dot != std::string::npos) text.replace(dot, 1, "playshelf/games/decimal_mark"_i18n);
    return text;
}

// The download's dialog (cadran::progress_body): what is under way (the
// download, then the checks, then the install), the build, a bar, the share
// and "3,2 of 4,7 MB · 1,1 MB/s", what it replaces, and Cancel (B too),
// updated from the download thread (at most once per percent, through
// aurore::sync). Cancel sets the flag the download checks (it stops within a
// second) and closes the dialog at once; nothing touches the dialog after it
// closed, whichever way (Cancel, B, the download's end) it did. Once the
// build is being put in place (installing()), Cancel goes: nothing can stop
// that.
class DownloadProgress
{
  public:
    DownloadProgress(const std::string& name, uint64_t bytes, const std::string& replaces) : size(bytes)
    {
        this->body = cadran::progress_body("playshelf/dev_build/downloading"_i18n, name, replaces);
        // Its own button, not the dialog's: a dialog's button acts once the
        // dialog is gone, and the download must stop updating it before.
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
        this->dialog->setCancelable(false);   // B: below, through cancel()
        this->dialog->getAppletFrame()->registerAction(
            "hints/cancel"_i18n, aurore::BUTTON_B, [this](aurore::View*) {
                this->cancel();
                return true;
            },
            false, false, aurore::SOUND_BACK);
        this->show(0, bytes);
        this->dialog->open();
    }

    // From the download thread.
    void received(uint64_t done, uint64_t total)
    {
        if (!total) total = this->size;
        const int now = total ? (int)std::min<uint64_t>(100, done * 100 / total) : 0;
        if (now == this->percent.exchange(now)) return;
        // `this` outlives every update: the shared_ptr is held by the
        // close() task, queued after them.
        aurore::sync([this, done, total]() { this->show(done, total); });
    }

    // What the download checks.
    const std::atomic<bool>* cancel_flag() const { return &this->cancelled; }
    bool was_cancelled() const { return this->cancelled.load(); }

    // The build is put in place: said, and Cancel goes.
    void installing()
    {
        if (!this->dialog) return;
        this->placing = true;
        this->body.heading->setText("playshelf/dev_build/installing"_i18n);
        this->body.show(1.0f, "");
        ui::set_visible(this->button, false);
    }

    void close()
    {
        if (this->dialog) this->dialog->close();
        this->dialog = nullptr;
    }

  private:
    const uint64_t size;
    std::atomic<int> percent{ -1 };
    std::atomic<bool> cancelled{ false };
    // The first bytes seen, and when: the speed is counted from them (a
    // download resumed starts with what an earlier attempt kept).
    uint64_t first_done = 0;
    std::chrono::steady_clock::time_point first_at;
    bool first_seen = false;
    aurore::Dialog* dialog = nullptr;   // null once it is closing: never touched again
    aurore::Button* button = nullptr;
    bool placing = false;   // installing(): nothing to cancel any more
    cadran::Progress body;

    void cancel()
    {
        if (!this->dialog || this->placing) return;
        this->cancelled = true;
        this->close();
    }

    void show(uint64_t done, uint64_t total)
    {
        if (!this->dialog) return;
        const float part = total ? std::min(1.0f, (float)done / (float)total) : 0.0f;
        // Everything in: what follows is the checks (the digest, the zip, the NRO header).
        if (total && done >= total) this->body.heading->setText("playshelf/dev_build/verifying"_i18n);
        std::string detail = aurore::getStr("playshelf/dev_build/downloaded_mb", mb_text(std::min(done, total)), mb_text(total));
        // The speed, once a second has gone by and while it is coming in.
        const auto now = std::chrono::steady_clock::now();
        if (!this->first_seen && done > 0) {
            this->first_seen = true;
            this->first_done = done;
            this->first_at = now;
        }
        const double elapsed = this->first_seen ? std::chrono::duration<double>(now - this->first_at).count() : 0;
        if (elapsed >= 1 && done > this->first_done && done < total)
            detail += " · " + aurore::getStr("playshelf/dev_build/speed",
                                          mb_text((uint64_t)((double)(done - this->first_done) / elapsed)));
        this->body.show(part, detail);
    }
};

void installed_now(const dev_builds::Build& b)
{
    // The build replaced is kept, and the next start checked (rollback_flow).
    rollback_flow::installed(label(b));
    // Not a newer release: its layout may be an earlier one, the migrations
    // run again when a later build is back (util/migrations.hpp).
    const std::string version = b.kind == dev_builds::Kind::Release ? b.version : "";
    if (migrations::is_earlier(version, app::version())) {
        aurore::Logger::info("dev builds: {} is not a newer release than {}: the migrations run again after it",
                           version.empty() ? "a build of a branch, a pull request or a commit" : version, app::version());
        migrations::earlier_build_next(paths::migrations_file(), [](bool warning, const std::string& line) {
            if (warning)
                aurore::Logger::warning("dev builds: {}", line);
            else
                aurore::Logger::info("dev builds: {}", line);
        });
    } else {
        aurore::Logger::info("dev builds: {} is a newer release than {}: the migrations recorded are kept", version,
                           app::version());
    }
    if (launcher::can_launch() && launcher::launch_nro(sd_path())) {
        // hbloader starts the new .nro as soon as this one exits.
        aurore::Application::quit();
        return;
    }
    ui::info(aurore::getStr("playshelf/dev_build/done_manual", label(b)));
}

void install(const dev_builds::Build& b)
{
    if (sd_path().empty()) {
        ui::info("playshelf/dev_build/no_path"_i18n);
        return;
    }
    if (s_busy) return;
    s_busy = true;
    const std::string target = file_path(sd_path());
    // What it replaces: the running build's name, as Go back to the previous
    // build will give it (rollback_flow).
    auto progress = std::make_shared<DownloadProgress>(label(b), b.size,
                                                       aurore::getStr("playshelf/dev_build/replacing", rollback_flow::running_label()));
    // Not behind the play log or the game icons (ui::in_background).
    ui::in_background("dev build download", [b, target, progress]() {
        std::string err;
        const std::string fresh = target + ".new";
        bool ok = dev_builds::download(b, fresh, &err, [progress](uint64_t done, uint64_t total) {
            progress->received(done, total);
        }, progress->cancel_flag());
        aurore::sync([b, ok, err, target, fresh, progress]() mutable {
            // Cancelled: nothing replaced, even when the download had just
            // ended; said with a toast (the dialog is already gone).
            if (progress->was_cancelled()) {
                std::remove(fresh.c_str());
                s_busy = false;
                ui::notify("playshelf/dev_build/cancelled"_i18n);
                return;
            }
            auto end = [b, progress](bool done, std::string why) {
                s_busy = false;
                progress->close();
                // The one reason a user can act on, said in their language.
                if (why == dev_builds::NO_DIGEST) why = "playshelf/dev_build/no_digest"_i18n;
                if (!done) ui::error(aurore::getStr("playshelf/dev_build/failed", why));
                else installed_now(b);
            };
            if (!ok) {
                end(false, err);
                return;
            }
            // Said first, drawn once, then done: the swap holds the UI thread.
            progress->installing();
            aurore::delay(100, [target, fresh, end]() {
                // The running .nro is held open by romfs:/ (the app's
                // resources), and the SD card will not rename an open file:
                // released for the two renames, here on the UI thread so that
                // nothing reads romfs:/ meanwhile, then mounted again.
                std::string replace_err;
                platform_romfs_release();
                const bool replaced = dev_builds::replace(target, fresh, &replace_err, rollback::previous_file(target));
                platform_romfs_mount();
                if (!replaced) std::remove(fresh.c_str());
                end(replaced, replace_err);
            });
        });
    });
}

}   // namespace

std::string nro_sd_path()
{
    return sd_path();
}

std::string nro_file()
{
    const std::string sd = sd_path();
    return sd.empty() ? "" : file_path(sd);
}

int64_t now()
{
#ifndef __SWITCH__
    // Desktop simulation: the console's frozen time, so ages stay the same in screenshots.
    if (const long long frozen = sim_env_int("PLAYSHELF_SIM_NOW", 0, 0, INT64_MAX)) return frozen;
#endif
    return (int64_t)std::time(nullptr);
}

std::string age_text(const std::string& iso)
{
    const dev_builds::Age age = dev_builds::age_of(iso, now());
    switch (age.unit) {
        case dev_builds::Age::None: break;
        case dev_builds::Age::Now: return "playshelf/dev_build/age_now"_i18n;
        case dev_builds::Age::Minutes: return aurore::getStr("playshelf/dev_build/age_minutes", age.count);
        case dev_builds::Age::Hours: return aurore::getStr("playshelf/dev_build/age_hours", age.count);
        case dev_builds::Age::Days: return aurore::getStr("playshelf/dev_build/age_days", age.count);
    }
    return "";
}

std::string label(const dev_builds::Build& b)
{
    std::string text;
    switch (b.kind) {
        case dev_builds::Kind::Release:
            text = release_text(b);
            break;
        case dev_builds::Kind::Main:
            text = aurore::getStr("playshelf/dev_build/main", b.commit, dev_builds::short_date(b.date));
            break;
        case dev_builds::Kind::PullRequest:
            text = aurore::getStr("playshelf/dev_build/pr", b.pr, subject(b), b.commit);
            break;
        case dev_builds::Kind::Commit:
            text = aurore::getStr("playshelf/dev_build/commit", b.commit, b.branch, dev_builds::short_date(b.date));
            break;
    }
    if (installed(b)) text += " · " + "playshelf/dev_build/installed"_i18n;
    return text;
}

const dev_builds::Build* running_build(const std::vector<dev_builds::Build>& all)
{
    for (const dev_builds::Build& b : all)
        if (b.kind != dev_builds::Kind::Release && installed(b)) return &b;
    return nullptr;
}

bool installed(const dev_builds::Build& b)
{
    // A release is told by its version (a release build has no commit).
    if (b.kind == dev_builds::Kind::Release) return app::is_release() && b.version == app::version();
    return !app::commit().empty() && b.commit == app::commit();
}

std::string release_text(const dev_builds::Build& b)
{
    std::string text = aurore::getStr("playshelf/dev_build/release", b.version);
    if (b.prerelease) text += " · " + "playshelf/dev_build/prerelease"_i18n;
    return text;
}

std::string checked_text(const dev_builds::Cache& cache)
{
    const int64_t age = now() - cache.fetched_at;
    if (age < 60) return "playshelf/dev_build/checked_now"_i18n;
    return aurore::getStr("playshelf/dev_build/checked", (int)(age / 60));
}

std::string subject(const dev_builds::Build& b)
{
    // A list kept by 1.1.0, which gave only pull requests a title.
    // Every list fetched from 1.2.0 on has them: this can go in 1.3.0.
    if (b.title.empty()) return b.commit;
    const std::string text = text::capitalise_ascii(dev_builds::parse_subject(b.title).text);
    return text.empty() ? b.title : text;
}

void choose(const dev_builds::Build& b, bool newer)
{
    DevBuildActivity::show(b, newer);
}

void install_now(const dev_builds::Build& b)
{
    // Nothing to check it against: refused before the PIN and the download
    // (its screen hides Install then).
    if (b.sha256.empty()) {
        ui::info("playshelf/dev_build/no_digest"_i18n);
        return;
    }
    const auto& may = ui::app_hooks::get().may_replace_self;
    if (may && !may()) return;
    install(b);
}

void sign_in()
{
    github_login_flow::sign_in([]() { open(); });
}

void go_back()
{
    std::string previous;
    if (s_busy || !rollback_flow::previous(&previous)) return;
    const std::string running = rollback_flow::running_label();
    ui::confirm(aurore::getStr("playshelf/dev_build/back_body", running, previous), "playshelf/dev_build/back_confirm"_i18n,
                []() {
                    const auto& may = ui::app_hooks::get().may_replace_self;
                    if (may && !may()) return;
                    rollback_flow::go_back_now();
                });
}

void find()
{
    if (s_busy) {
        ui::notify("playshelf/dev_build/busy"_i18n);
        return;
    }
    ui::prompt_text("playshelf/dev_build/find_header"_i18n, "", 40, [](std::string text) {
        dev_builds::Query q;
        if (!dev_builds::parse_query(text, &q)) {
            ui::info(aurore::getStr("playshelf/dev_build/find_invalid", text));
            return;
        }
        if (s_busy) return;
        s_busy = true;
        const std::string what = q.pr ? "#" + std::to_string(q.pr) : q.commit.substr(0, 7);
        // A few requests, a few seconds: the bar fills at a steady pace meanwhile.
        DevBuildsActivity::loading(aurore::getStr("playshelf/dev_build/finding", what), 0.0f, true);
        ui::in_background("dev build find", [q]() {
            dev_builds::Build b;
            std::string err;
            const bool ok = dev_builds::find(q, &b, &err);
            aurore::sync([ok, b, err]() {
                s_busy = false;
                DevBuildsActivity::loaded();
                if (ok) choose(b);
                else ui::error(aurore::getStr("playshelf/dev_build/find_failed", err));
            });
        });
    });
}

void refresh()
{
    if (!s_busy) open(true);
}

void open(bool force)
{
    if (s_busy) {
        ui::notify("playshelf/dev_build/busy"_i18n);
        return;
    }
    // A list fetched a few minutes ago, in the same sign-in state, is shown at
    // once; an older one is fetched again (and still shown if GitHub cannot
    // be reached).
    read_cache();
    const bool needs_login = github_auth::token().empty();
    if (!force && dev_builds::cache_fresh(s_cache, now(), needs_login)) {
        DevBuildsActivity::show(s_cache);
        return;
    }
    s_busy = true;
    aurore::Logger::info("dev builds: fetching the list");
    // The screen opens at once, the list kept (if any) under a bar that shows
    // how far the fetch is: a toast alone looked like nothing was happening.
    // Nothing kept yet: an empty list, Sign in already there when signed out.
    dev_builds::Cache shown = s_cache;
    if (shown.builds.empty()) shown.needs_login = needs_login;
    DevBuildsActivity::show(shown);
    DevBuildsActivity::loading("playshelf/dev_build/step_releases"_i18n, 0.0f);
    // Not behind the play log or the game icons (ui::in_background).
    ui::in_background("dev build list", []() {
        dev_builds::Cache fresh;
        std::string err;
        const bool ok = dev_builds::fetch(&fresh.builds, &fresh.needs_login, &err, [](float done, dev_builds::Step step) {
            aurore::sync([done, step]() {
                const char* key = step == dev_builds::Step::Releases    ? "playshelf/dev_build/step_releases"
                                  : step == dev_builds::Step::Artifacts ? "playshelf/dev_build/step_artifacts"
                                  : step == dev_builds::Step::Pulls     ? "playshelf/dev_build/step_pulls"
                                                                        : "playshelf/dev_build/step_runs";
                DevBuildsActivity::loading(aurore::getStr(key), done);
            });
        });
        fresh.fetched_at = now();
        aurore::sync([ok, fresh, err]() {
            s_busy = false;
            DevBuildsActivity::loaded();
            if (ok) {
                keep(fresh);
                DevBuildsActivity::show(fresh);
            } else if (!s_cache.builds.empty()) {
                ui::notify(aurore::getStr("playshelf/dev_build/list_stale", err));
                DevBuildsActivity::show(s_cache);
            } else if (fresh.needs_login) {
                DevBuildsActivity::show(fresh);   // signing in may still list them
            } else {
                DevBuildsActivity::failed(aurore::getStr("playshelf/dev_build/list_failed", err));
            }
        });
    });
}

}   // namespace dev_build_flow
