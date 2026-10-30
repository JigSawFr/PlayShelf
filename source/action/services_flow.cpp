// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/services_flow.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <aurore.hpp>
#include <cstdio>
#include <map>
#include <set>

#include <fmt/format.h>

#include "action/titledb_flow.hpp"
#include "core/saves.h"
#include "library/http_transport.hpp"
#include "library/servers.hpp"
#include "library/services.hpp"
#include "library/titles.hpp"
#include "saves/archive.hpp"
#include "ui/ui.hpp"
#include "util/number.hpp"
#include "util/paths.hpp"
#include "util/title_id.hpp"

using namespace aurore::literals;

namespace services_flow
{
namespace
{
using Loaded = std::shared_ptr<const library_flow::Loaded>;
using Choice = ui::Choice;

// The web sign-in of each server, by its id: the UI thread's only.
std::map<std::string, library::Session> s_sessions;

library::Link link_of(const library_flow::Loaded& l)
{
    return library::link_of(l.server, l.fetch, library::password_of(l.server));
}

// A game's name: the catalogue's, the title databases', else `fallback`,
// else its title id.
std::string game_name(const library_flow::Loaded& l, uint64_t id, const std::string& fallback = "")
{
    for (const library::Title& t : l.titles)
        if (t.id == id && !t.name.empty()) return t.name;
    if (const auto t = titledb_flow::title(id); t && !t->name.empty()) return t->name;
    return fallback.empty() ? title_id::text(id) : fallback;
}

// A dialog while something runs in the background: its line below says how
// far it got; a Cancel button when it may be stopped.
struct Busy
{
    aurore::Dialog*                      dialog = nullptr;
    aurore::Label*                       line = nullptr;
    std::shared_ptr<std::atomic<bool>> cancel = std::make_shared<std::atomic<bool>>(false);
    int                                shown = -1;   // the percentage on its line
};

std::shared_ptr<Busy> busy(const std::string& text, bool cancellable)
{
    auto b = std::make_shared<Busy>();
    b->line = new aurore::Label();
    b->line->setHorizontalAlign(aurore::HorizontalAlign::CENTER);
    b->line->setText(" ");
    b->dialog = ui::dialog(text, b->line);
    if (cancellable) {
        auto cancel = b->cancel;
        ui::add_cancel(b->dialog, [cancel]() { *cancel = true; });
    }
    b->dialog->open();
    return b;
}

// From any thread: the line says `done` of `total` (a percentage).
void show_progress(const std::shared_ptr<Busy>& b, uint64_t done, uint64_t total)
{
    if (!total) return;
    const int pct = (int)std::min<uint64_t>(100, done * 100 / total);
    aurore::sync([b, pct, done, total]() {
        if (*b->cancel || pct == b->shown) return;
        b->shown = pct;
        b->line->setText(fmt::format("{} %  ·  {} / {}", pct, library_flow::size_text(done), library_flow::size_text(total)));
    });
}

// Closes the dialog; false when it was cancelled (closed already).
bool finish(const std::shared_ptr<Busy>& b)
{
    if (*b->cancel) return false;
    b->dialog->close();
    return true;
}

// ---------------------------------------------------------------- cheats

void cheat_sheet(const Loaded& l, uint64_t title_id, const std::vector<library::Cheat>& builds);

// `chosen` installed (fetched, then written whole); `after` then.
void install_cheats(const Loaded& l, const std::vector<library::Cheat>& chosen, const std::function<void()>& after)
{
    if (ui::refuse_read_only() || chosen.empty()) return;
    const library::Link link = link_of(*l);
    auto                b = busy("playshelf/services/cheats_installing"_i18n, false);
    ui::in_background("cheats", [b, link, chosen, after]() {
        library::HttpTransport t;
        size_t                 done = 0;
        std::string            why;
        for (const library::Cheat& c : chosen) {
            std::string text;
            const std::string path = library::cheat_path(paths::sd_root(), c.title_id, c.build_id);
            if (!library::fetch_cheat(link, t, c, &text, &why)) break;
            if (path.empty() || !paths::ensure_dir(path.substr(0, path.find_last_of('/'))) ||
                !paths::atomic_write(path, text, &why)) {
                if (why.empty()) why = "the SD card refused the file";
                break;
            }
            done++;
            show_progress(b, done, chosen.size());
        }
        aurore::sync([b, done, total = chosen.size(), why, after]() {
            finish(b);
            if (done < total) ui::error("playshelf/services/cheats_failed"_i18n, why);
            else ui::notify(total == 1 ? "playshelf/services/cheat_installed"_i18n
                                       : aurore::getStr("playshelf/services/cheats_installed", total));
            if (after) after();
        });
    });
}

void view_cheat(const Loaded& l, const library::Cheat& c)
{
    const library::Link link = link_of(*l);
    auto                b = busy("playshelf/services/cheat_reading"_i18n, true);
    ui::in_background("cheat", [b, link, c]() {
        library::HttpTransport t;
        std::string            text, why;
        const bool             ok = library::fetch_cheat(link, t, c, &text, &why);
        aurore::sync([b, ok, text, why, c]() {
            if (!finish(b)) return;
            if (!ok) {
                ui::error("playshelf/services/cheat_failed"_i18n, why);
                return;
            }
            // A screen's worth, line by line: the codes themselves are no
            // reading matter.
            std::string shown;
            size_t      at = 0, lines = 0;
            while (at < text.size() && lines < 80) {
                const size_t end = text.find('\n', at);
                shown += library::display_text(text.substr(at, end == std::string::npos ? std::string::npos : end - at), 160) + "\n";
                at = end == std::string::npos ? text.size() : end + 1;
                lines++;
            }
            if (at < text.size()) shown += "…";
            ui::info(aurore::getStr("playshelf/services/build", c.build_id) + "\n\n" + shown);
        });
    });
}

void remove_cheat(uint64_t title_id, const std::string& build_id, const std::function<void()>& after)
{
    if (ui::refuse_read_only()) return;
    ui::confirm_danger(aurore::getStr("playshelf/services/cheat_remove_body", build_id), "playshelf/services/cheat_remove"_i18n,
                       [title_id, build_id, after]() {
                           const std::string path = library::cheat_path(paths::sd_root(), title_id, build_id);
                           if (path.empty() || std::remove(path.c_str()) != 0) ui::error("playshelf/services/cheat_remove_failed"_i18n);
                           else ui::notify("playshelf/services/cheat_removed"_i18n);
                           if (after) after();
                       });
}

void build_sheet(const Loaded& l, uint64_t title_id, const std::vector<library::Cheat>& builds, const library::Cheat& c)
{
    const auto have = library::installed_cheats(paths::sd_root(), title_id);
    const bool installed = std::ranges::contains(have, c.build_id);
    auto       again = [l, title_id, builds]() { cheat_sheet(l, title_id, builds); };
    std::vector<Choice> choices;
    choices.emplace_back(installed ? "playshelf/services/cheat_reinstall"_i18n : "playshelf/services/cheat_install"_i18n,
                         [l, c, again]() { install_cheats(l, { c }, again); }, "download");
    choices.emplace_back("playshelf/services/cheat_view"_i18n, [l, c]() { view_cheat(l, c); }, "eye");
    if (installed)
        choices.emplace_back("playshelf/services/cheat_remove"_i18n,
                             [title_id, c, again]() { remove_cheat(title_id, c.build_id, again); }, "trash", "", "",
                             Choice::Tone::DANGER);
    ui::pick_actions(aurore::getStr("playshelf/services/build", c.build_id), choices,
                     c.note.empty() ? game_name(*l, title_id, c.title_name) : c.note);
}

// A game's builds on the server, and those the SD card has of it besides.
void cheat_sheet(const Loaded& l, uint64_t title_id, const std::vector<library::Cheat>& builds)
{
    const auto           have = library::installed_cheats(paths::sd_root(), title_id);
    std::vector<Choice>  choices;
    std::vector<library::Cheat> missing;
    for (const library::Cheat& c : builds)
        if (!std::ranges::contains(have, c.build_id)) missing.push_back(c);
    if (missing.size() > 1)
        choices.emplace_back("playshelf/services/cheats_install_all"_i18n,
                             [l, missing, title_id, builds]() { install_cheats(l, missing, [l, title_id, builds]() { cheat_sheet(l, title_id, builds); }); },
                             "download", aurore::getStr("playshelf/services/cheats_count", missing.size()));
    for (const library::Cheat& c : builds) {
        const bool on = std::ranges::contains(have, c.build_id);
        choices.emplace_back(aurore::getStr("playshelf/services/build", c.build_id),
                             [l, title_id, builds, c]() { build_sheet(l, title_id, builds, c); }, "code",
                             on ? "playshelf/services/cheat_on_console"_i18n : c.note,
                             "playshelf/services/group_server_builds"_i18n, on ? Choice::Tone::ON : Choice::Tone::PLAIN);
    }
    for (const std::string& bid : have) {
        if (std::any_of(builds.begin(), builds.end(), [&](const library::Cheat& c) { return c.build_id == bid; })) continue;
        choices.emplace_back(aurore::getStr("playshelf/services/build", bid),
                             [title_id, bid, l, builds]() { remove_cheat(title_id, bid, [l, title_id, builds]() { cheat_sheet(l, title_id, builds); }); },
                             "code", "playshelf/services/cheat_remove"_i18n, "playshelf/services/group_console_builds"_i18n,
                             Choice::Tone::DANGER);
    }
    if (choices.empty()) {
        ui::info("playshelf/services/cheats_none_game"_i18n);
        return;
    }
    const std::string fallback = builds.empty() ? "" : builds.front().title_name;
    ui::pick_actions(game_name(*l, title_id, fallback), choices, title_id::text(title_id) + " · " + "playshelf/services/cheats_note"_i18n);
}

// ---------------------------------------------------------------- saves

struct SaveData
{
    CoreSaveUser                     user{};
    std::set<uint64_t>               local;     // the games this user has a save of
    bool                             local_read = false;
    std::vector<library::SaveBackup> backups;   // the server's
    std::string                      why;       // the server's list could not be read
};

void open_saves(const Loaded& l, const CoreSaveUser& user, uint64_t only);
void game_saves(const Loaded& l, const std::shared_ptr<const SaveData>& d, uint64_t title_id);

// On a worker thread: the games `user` has a save of, and the server's backups.
std::shared_ptr<const SaveData> read_saves(const library::Link& link, const CoreSaveUser& user)
{
    auto d = std::make_shared<SaveData>();
    d->user = user;
    std::vector<u64> ids(4096);
    const int        n = core_save_titles(&user, ids.data(), (int)ids.size());
    d->local_read = n >= 0;
    for (int i = 0; i < n; i++) d->local.insert(ids[(size_t)i]);
    library::HttpTransport t;
    if (!library::list_saves(link, t, &d->backups, &d->why)) d->backups.clear();
    return d;
}

std::string backup_label(const library::SaveBackup& s)
{
    std::string when = s.created ? ui::time_text((uint64_t)s.created) : s.created_at;
    if (s.server_copy) return aurore::getStr("playshelf/services/save_server_copy", when);
    return s.note.empty() ? when : when + " · " + s.note;
}

void back_up(const Loaded& l, const CoreSaveUser& user, uint64_t title_id, const std::string& note)
{
    const library::Link link = link_of(*l);
    auto                b = busy(aurore::getStr("playshelf/services/save_sending", game_name(*l, title_id)), true);
    ui::in_background("save backup", [b, link, user, title_id, note, l]() {
        std::string       why;
        const std::string zip = paths::cache_dir() + "/save_upload.zip";
        paths::ensure_dir(paths::cache_dir());
        char root[512];
        bool ok = R_SUCCEEDED(core_save_mount(&user, title_id, false, root, sizeof root));
        if (!ok) why = "playshelf/services/save_mount_failed"_i18n;
        if (ok) {
            ok = save_archive::zip_dir(root, zip, nullptr, &why);
            core_save_unmount();
        }
        if (ok && !*b->cancel) {
            library::HttpTransport t;
            ok = library::upload_save(link, t, title_id, zip, note,
                                      [b](uint64_t done, uint64_t total) { show_progress(b, done, total); },
                                      b->cancel.get(), &why);
        }
        std::remove(zip.c_str());
        // The game's backups again, in the same go (no other dialog opening
        // while this one closes).
        const auto d = ok ? read_saves(link, user) : nullptr;
        aurore::sync([b, ok, why, l, d, title_id]() {
            if (!finish(b)) return;
            if (!ok) {
                ui::error("playshelf/services/save_send_failed"_i18n, why);
                return;
            }
            ui::notify("playshelf/services/save_sent"_i18n);
            game_saves(l, d, title_id);
        });
    });
}

// The save on the console replaced by `backup`: the archive fetched and
// checked; the save as it is kept on the SD card; the save emptied and the
// archive written in, committed file by file; the save kept put back should
// that fail.
void restore(const Loaded& l, const CoreSaveUser& user, const library::SaveBackup& backup)
{
    if (ui::refuse_read_only()) return;
    const library::Link link = link_of(*l);
    const std::string   name = game_name(*l, backup.title_id, backup.title_name);
    auto                b = busy(aurore::getStr("playshelf/services/save_restoring", name), false);
    ui::in_background("save restore", [b, link, user, backup, l, name]() {
        std::string       why, kept;
        const std::string zip = paths::cache_dir() + "/save_restore.zip";
        paths::ensure_dir(paths::cache_dir());
        library::HttpTransport          t;
        std::vector<save_archive::Entry> entries;
        bool ok = library::download_save(link, t, backup, zip, [b](uint64_t d, uint64_t n) { show_progress(b, d, n); },
                                         nullptr, &why) &&
                  save_archive::read_entries(zip, &entries, &why);
        char root[512];
        if (ok) {
            // The save as it is, kept first: nothing is replaced without it.
            const std::string dir = paths::data_dir() + "/saves/" + title_id::text(backup.title_id);
            std::string stamp = ui::today_date() + "_" + ui::now_hms();
            std::replace(stamp.begin(), stamp.end(), ':', '-');
            kept = dir + "/" + stamp + ".zip";
            ok = paths::ensure_dir(dir) && R_SUCCEEDED(core_save_mount(&user, backup.title_id, false, root, sizeof root));
            if (!ok) why = "playshelf/services/save_mount_failed"_i18n;
            if (ok) {
                ok = save_archive::zip_dir(root, kept, nullptr, &why);
                core_save_unmount();
            }
            if (!ok) why = aurore::getStr("playshelf/services/save_keep_failed", why);
        }
        bool put_back = false;
        if (ok) {
            ok = R_SUCCEEDED(core_save_mount(&user, backup.title_id, true, root, sizeof root));
            if (!ok) why = "playshelf/services/save_mount_failed"_i18n;
            if (ok) {
                const auto commit = [](const std::string&) { return R_SUCCEEDED(core_save_commit()); };
                ok = save_archive::clear_dir(root, &why) && R_SUCCEEDED(core_save_commit()) &&
                     save_archive::unzip_to(zip, root, commit, nullptr, &why) && R_SUCCEEDED(core_save_commit());
                if (!ok) {
                    // The save as it was, back.
                    std::string again;
                    put_back = save_archive::clear_dir(root, &again) && save_archive::unzip_to(kept, root, commit, nullptr, &again) &&
                               R_SUCCEEDED(core_save_commit());
                }
                core_save_unmount();
            }
        }
        std::remove(zip.c_str());
        aurore::sync([b, ok, why, kept, put_back, name]() {
            finish(b);
            if (!ok) {
                std::string before;
                if (put_back) before = "playshelf/services/save_put_back"_i18n;
                else if (!kept.empty()) before = aurore::getStr("playshelf/services/save_kept_at", kept);
                ui::error("playshelf/services/save_restore_failed"_i18n, before.empty() ? why : before + "\n\n" + why);
                return;
            }
            ui::info(aurore::getStr("playshelf/services/save_restored", name, kept));
        });
    });
}

void remove_backup(const Loaded& l, const CoreSaveUser& user, const library::SaveBackup& backup)
{
    ui::confirm_danger(aurore::getStr("playshelf/services/save_delete_body", backup_label(backup)),
                       "playshelf/services/save_delete"_i18n, [l, user, backup]() {
                           const library::Link link = link_of(*l);
                           auto                b = busy("playshelf/services/save_deleting"_i18n, false);
                           ui::in_background("save delete", [b, link, backup, l, user]() {
                               library::HttpTransport t;
                               std::string            why;
                               const bool             ok = library::delete_save(link, t, backup, &why);
                               const auto             d = ok ? read_saves(link, user) : nullptr;
                               aurore::sync([b, ok, why, l, d, backup]() {
                                   finish(b);
                                   if (!ok) ui::error("playshelf/services/save_delete_failed"_i18n, why);
                                   else game_saves(l, d, backup.title_id);
                               });
                           });
                       });
}

void game_saves(const Loaded& l, const std::shared_ptr<const SaveData>& d, uint64_t title_id)
{
    std::vector<Choice> choices;
    const bool          here = d->local.contains(title_id);
    const CoreSaveUser  user = d->user;
    if (here) {
        choices.emplace_back("playshelf/services/save_send"_i18n, [l, user, title_id]() { back_up(l, user, title_id, ""); },
                             "upload", "", "playshelf/services/group_console"_i18n);
        // The keyboard closes first (a dialog of its own on the desktop).
        choices.emplace_back("playshelf/services/save_send_note"_i18n,
                             [l, user, title_id]() {
                                 ui::prompt_text("playshelf/services/save_note"_i18n, "", (int)library::MAX_NOTE,
                                                 [l, user, title_id](const std::string& note) {
                                                     aurore::sync([l, user, title_id, note]() { back_up(l, user, title_id, note); });
                                                 });
                             },
                             "pencil", "", "playshelf/services/group_console"_i18n);
    } else
        choices.emplace_back("playshelf/services/save_none_here"_i18n, nullptr, "info", "", "playshelf/services/group_console"_i18n);
    std::string fallback;
    size_t      count = 0;
    for (const library::SaveBackup& s : d->backups) {
        if (s.title_id != title_id) continue;
        fallback = s.title_name;
        count++;
        choices.emplace_back(backup_label(s),
                             [l, user, s, here]() {
                                 std::vector<Choice> what;
                                 if (here)
                                     what.emplace_back("playshelf/services/save_restore"_i18n, [l, user, s]() {
                                         ui::confirm_danger(aurore::getStr("playshelf/services/save_restore_body", game_name(*l, s.title_id, s.title_name),
                                                                         user.name, backup_label(s)),
                                                            "playshelf/services/save_restore"_i18n, [l, user, s]() { restore(l, user, s); });
                                     }, "restore");
                                 else
                                     what.emplace_back("playshelf/services/save_play_first"_i18n, nullptr, "info");
                                 if (!s.delete_url.empty())
                                     what.emplace_back("playshelf/services/save_delete"_i18n, [l, user, s]() { remove_backup(l, user, s); },
                                                       "trash", "", "", Choice::Tone::DANGER);
                                 ui::pick_actions(backup_label(s), what, library_flow::size_text(s.size));
                             },
                             s.server_copy ? "folder" : "save", library_flow::size_text(s.size), "playshelf/services/group_server"_i18n,
                             s.latest ? Choice::Tone::ON : Choice::Tone::PLAIN);
    }
    if (!count)
        choices.emplace_back(d->why.empty() ? "playshelf/services/save_none_server"_i18n : d->why, nullptr, "info", "",
                             "playshelf/services/group_server"_i18n);
    ui::pick_actions(game_name(*l, title_id, fallback), choices, aurore::getStr("playshelf/services/save_user", d->user.name));
}

void games_with_saves(const Loaded& l, const std::shared_ptr<const SaveData>& d)
{
    std::map<uint64_t, size_t> backups;
    for (const library::SaveBackup& s : d->backups)
        backups[s.title_id] += !s.server_copy;
    std::set<uint64_t> ids = d->local;
    for (const auto& kv : backups) ids.insert(kv.first);
    if (ids.empty()) {
        ui::info(d->why.empty() ? "playshelf/services/saves_none"_i18n : aurore::getStr("playshelf/services/saves_failed", d->why));
        return;
    }
    std::vector<std::pair<std::string, uint64_t>> games;
    for (uint64_t id : ids) {
        std::string fallback;
        for (const library::SaveBackup& s : d->backups)
            if (s.title_id == id) fallback = s.title_name;
        games.emplace_back(game_name(*l, id, fallback), id);
    }
    library::sort_folded(games, [](const auto& g) -> const std::string& { return g.first; });
    std::vector<std::string> names, details;
    for (const auto& [name, id] : games) {
        names.push_back(name);
        const size_t n = backups.count(id) ? backups.at(id) : 0;
        std::string  detail = d->local.count(id) ? "playshelf/services/save_here"_i18n : "playshelf/services/save_not_here"_i18n;
        detail += " · " + (n == 0   ? "playshelf/services/no_backup"_i18n
                           : n == 1 ? "playshelf/services/backups_one"_i18n
                                    : aurore::getStr("playshelf/services/backups", n));
        details.push_back(detail);
    }
    ui::pick(aurore::getStr("playshelf/services/saves_of", d->user.name), names, 0,
             [l, d, games](int i) { game_saves(l, d, games[(size_t)i].second); }, false, details);
}

void open_saves(const Loaded& l, const CoreSaveUser& user, uint64_t only)
{
    const library::Link link = link_of(*l);
    auto                b = busy("playshelf/services/saves_reading"_i18n, true);
    ui::in_background("saves", [b, link, user, only, l]() {
        const auto d = read_saves(link, user);
        aurore::sync([b, d, l, only]() {
            if (!finish(b)) return;
            if (only) game_saves(l, d, only);
            else games_with_saves(l, d);
        });
    });
}

// ---------------------------------------------------------------- requests

void ask_for(const Loaded& l, uint64_t id, const std::string& name)
{
    const library::Link link = link_of(*l);
    const std::string   server_id = l->server.id;
    library::Session    session = s_sessions[server_id];
    auto                b = busy("playshelf/services/request_sending"_i18n, false);
    ui::in_background("request", [b, link, session, id, name, server_id]() mutable {
        library::HttpTransport t;
        std::string            message, why;
        const bool             ok = library::request_title(link, t, session, id, name, &message, &why);
        aurore::sync([b, ok, message, why, session, server_id]() {
            s_sessions[server_id] = session;
            finish(b);
            if (!ok) ui::error("playshelf/services/request_failed"_i18n, why);
            else ui::notify(message.empty() ? "playshelf/services/request_sent"_i18n : message);
        });
    });
}

void search_and_ask(const Loaded& l)
{
    ui::prompt_text("playshelf/services/request_search"_i18n, "", 64, [l](const std::string& query) {
        // A title id typed as it is.
        const uint64_t typed = query.size() == 16 ? number::parse<uint64_t>(query, 16).value_or(0) : 0;
        if (typed) {
            const uint64_t base = library::base_of(typed);
            ui::confirm(aurore::getStr("playshelf/services/request_body", game_name(*l, base)), "playshelf/services/request"_i18n,
                        [l, base]() {
                            // Its name only when known: never its title id in its place.
                            const std::string name = game_name(*l, base);
                            ask_for(l, base, name == title_id::text(base) ? "" : name);
                        });
            return;
        }
        auto b = busy("playshelf/services/request_searching"_i18n, true);
        ui::in_background("title search", [b, l, query]() {
            const auto found = titledb_flow::search(query, 60);
            aurore::sync([b, l, found]() {
                if (!finish(b)) return;
                if (found.empty()) {
                    ui::info("playshelf/services/request_nothing"_i18n);
                    return;
                }
                std::set<uint64_t> on_server;
                for (const library::Title& t : l->titles) on_server.insert(t.id);
                std::vector<std::string> names, details;
                for (const auto& [id, name] : found) {
                    names.push_back(name);
                    details.push_back(on_server.count(id) ? "playshelf/services/request_there"_i18n : title_id::text(id));
                }
                ui::pick("playshelf/services/request_pick"_i18n, names, 0, [l, found, on_server](int i) {
                    const auto& [id, name] = found[(size_t)i];
                    if (on_server.contains(id)) {
                        ui::info("playshelf/services/request_there_body"_i18n);
                        return;
                    }
                    ask_for(l, id, name);
                }, false, details);
            });
        });
    });
}

std::string status_text(const std::string& status)
{
    if (status == "open") return "playshelf/services/request_open"_i18n;
    if (status == "closed") return "playshelf/services/request_closed"_i18n;
    if (status == "denied") return "playshelf/services/request_denied"_i18n;
    return status;
}
}   // namespace

bool offered(const library_flow::Loaded& l)
{
    return l.fetch.status == library::FetchStatus::Ok && l.fetch.dialect == library::Dialect::Sections;
}

std::vector<Choice> game_choices(const Loaded& l, uint64_t title_id)
{
    std::vector<Choice> out;
    if (!l || !title_id || !offered(*l)) return out;
    out.emplace_back("playshelf/services/cheats"_i18n, [l, title_id]() { cheats(l, title_id); }, "code", "",
                     "playshelf/services/group_server_more"_i18n);
    out.emplace_back("playshelf/services/saves"_i18n, [l, title_id]() { saves(l, title_id); }, "save", "",
                     "playshelf/services/group_server_more"_i18n);
    return out;
}

void cheats(const Loaded& l, uint64_t title_id)
{
    if (!l) return;
    const library::Link link = link_of(*l);
    const std::string   server_id = l->server.id;
    library::Session    session = s_sessions[server_id];
    auto                b = busy("playshelf/services/cheats_reading"_i18n, true);
    ui::in_background("cheats", [b, link, session, server_id, l, title_id]() mutable {
        library::HttpTransport      t;
        std::vector<library::Cheat> list;
        std::string                 why;
        const bool                  ok = library::list_cheats(link, t, session, &list, &why);
        aurore::sync([b, ok, list, why, session, server_id, l, title_id]() {
            s_sessions[server_id] = session;
            if (!finish(b)) return;
            if (!ok) {
                ui::error("playshelf/services/cheats_failed_list"_i18n, why);
                return;
            }
            if (title_id) {
                std::vector<library::Cheat> mine;
                for (const library::Cheat& c : list)
                    if (c.title_id == title_id) mine.push_back(c);
                cheat_sheet(l, title_id, mine);
                return;
            }
            // By game: a list of them, each with how many builds.
            std::map<uint64_t, std::vector<library::Cheat>> by_game;
            for (const library::Cheat& c : list) by_game[c.title_id].push_back(c);
            if (by_game.empty()) {
                ui::info("playshelf/services/cheats_none"_i18n);
                return;
            }
            std::vector<std::pair<std::string, uint64_t>> games;
            for (const auto& [id, builds] : by_game) games.emplace_back(game_name(*l, id, builds.front().title_name), id);
            library::sort_folded(games, [](const auto& g) -> const std::string& { return g.first; });
            std::vector<std::string> names, details;
            for (const auto& [name, id] : games) {
                names.push_back(name);
                const size_t n = by_game[id].size();
                details.push_back(n == 1 ? "playshelf/services/builds_one"_i18n : aurore::getStr("playshelf/services/builds", n));
            }
            ui::pick("playshelf/services/cheats"_i18n, names, 0, [l, games, by_game](int i) {
                const uint64_t id = games[(size_t)i].second;
                cheat_sheet(l, id, by_game.at(id));
            }, false, details);
        });
    });
}

void saves(const Loaded& l, uint64_t title_id)
{
    if (!l) return;
    ui::in_background("users", [l, title_id]() {
        std::vector<CoreSaveUser> users(CORE_SAVE_USERS_MAX);
        const int                 n = core_save_users(users.data(), (int)users.size());
        users.resize(n > 0 ? (size_t)n : 0);
        aurore::sync([l, title_id, users]() {
            if (users.empty()) {
                ui::error("playshelf/services/saves_no_user"_i18n);
                return;
            }
            if (users.size() == 1) {
                open_saves(l, users[0], title_id);
                return;
            }
            std::vector<std::string> names;
            for (const CoreSaveUser& u : users) names.push_back(u.name[0] ? u.name : "?");
            ui::pick("playshelf/services/saves_user_pick"_i18n, names, 0,
                     [l, title_id, users](int i) { open_saves(l, users[(size_t)i], title_id); }, false);
        });
    });
}

void requests(const Loaded& l)
{
    if (!l) return;
    const library::Link link = link_of(*l);
    const std::string   server_id = l->server.id;
    library::Session    session = s_sessions[server_id];
    auto                b = busy("playshelf/services/requests_reading"_i18n, true);
    ui::in_background("requests", [b, link, session, server_id, l]() mutable {
        library::HttpTransport             t;
        std::vector<library::TitleRequest> list;
        std::string                        why;
        const bool                         ok = library::list_requests(link, t, session, &list, &why);
        aurore::sync([b, ok, list, why, session, server_id, l]() {
            s_sessions[server_id] = session;
            if (!finish(b)) return;
            if (!ok) {
                ui::error("playshelf/services/requests_failed"_i18n, why);
                return;
            }
            std::vector<Choice> choices;
            choices.emplace_back("playshelf/services/request_new"_i18n, [l]() { search_and_ask(l); }, "plus");
            for (const library::TitleRequest& r : list) {
                const Choice::Tone tone = r.status == "closed" ? Choice::Tone::ON
                                        : r.status == "denied" ? Choice::Tone::WARN
                                                               : Choice::Tone::PLAIN;
                choices.emplace_back(game_name(*l, r.title_id, r.title_name), nullptr, "gamepad", status_text(r.status),
                                     "playshelf/services/group_requests"_i18n, tone);
            }
            ui::pick_actions("playshelf/services/requests"_i18n, choices,
                             list.empty() ? "playshelf/services/requests_none"_i18n : "");
        });
    });
}

}   // namespace services_flow
