// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/local_saves_flow.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <aurore.hpp>

#include "core/saves.h"
#include "saves/archive.hpp"
#include "saves/local_backups.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"

using namespace aurore::literals;

namespace local_saves_flow
{

namespace
{
constexpr int MAX_TITLES = 2048;

bool has_save(const CoreSaveUser& user, uint64_t title_id)
{
    std::vector<u64> titles(MAX_TITLES);
    const int        n = core_save_titles(&user, titles.data(), MAX_TITLES);
    return n > 0 && std::find(titles.begin(), titles.begin() + n, (u64)title_id) != titles.begin() + n;
}
}   // namespace

Outcome back_up_game(uint64_t title_id)
{
    Outcome out;
    CoreSaveUser users[CORE_SAVE_USERS_MAX];
    const int    count = core_save_users(users, CORE_SAVE_USERS_MAX);
    if (count < 0) {
        out.error = "playshelf/saves_local/users_failed"_i18n;
        return out;
    }
    const std::string dir = local_backups::dir_of(paths::data_dir(), title_id);
    for (int i = 0; i < count; i++) {
        if (!has_save(users[i], title_id)) continue;
        if (!paths::ensure_dir(paths::data_dir() + "/saves") || !paths::ensure_dir(dir)) {
            if (out.error.empty()) out.error = "playshelf/saves_local/write_failed"_i18n;
            continue;
        }
        const std::string name = local_backups::file_name(ui::today_date(), ui::now_hms(), local_backups::user_tag(users[i].uid[0], users[i].uid[1]));
        const std::string part = dir + "/" + name + ".part", path = dir + "/" + name;
        char              root[512];
        std::string       why;
        bool              ok = R_SUCCEEDED(core_save_mount(&users[i], title_id, false, root, sizeof root));
        if (!ok) why = "playshelf/services/save_mount_failed"_i18n;
        if (ok) {
            ok = save_archive::zip_dir(root, part, nullptr, &why);
            core_save_unmount();
        }
        // Written beside, then put in place: a cut leaves no half copy under a name that counts.
        if (ok && std::rename(part.c_str(), path.c_str()) != 0) {
            ok = false;
            why = "playshelf/saves_local/write_failed"_i18n;
        }
        if (!ok) {
            std::remove(part.c_str());
            if (out.error.empty()) out.error = why;
            continue;
        }
        out.copies++;
    }
    // The oldest beyond the last few of each user go; only what this makes is ever counted.
    for (const std::string& name : local_backups::expired(paths::list_files(dir, ".zip"))) std::remove((dir + "/" + name).c_str());
    return out;
}

namespace
{
// The save of `user` for the game replaced by `zip`: the entries read first (nothing is touched when
// the archive is bad), the save as it is kept in `kept`, then emptied and the archive written in,
// committed file by file; the kept one put back should that fail. `put_back` says it was.
bool restore_copy(const CoreSaveUser& user, uint64_t title_id, const std::string& zip, const std::string& kept, bool* put_back,
                  std::string* why)
{
    std::vector<save_archive::Entry> entries;
    if (!save_archive::read_entries(zip, &entries, why)) return false;
    char root[512];
    if (!R_SUCCEEDED(core_save_mount(&user, title_id, false, root, sizeof root))) {
        *why = "playshelf/services/save_mount_failed"_i18n;
        return false;
    }
    const bool kept_ok = save_archive::zip_dir(root, kept, nullptr, why);
    core_save_unmount();
    if (!kept_ok) {
        *why = aurore::getStr("playshelf/services/save_keep_failed", *why);
        return false;
    }
    if (!R_SUCCEEDED(core_save_mount(&user, title_id, true, root, sizeof root))) {
        *why = "playshelf/services/save_mount_failed"_i18n;
        return false;
    }
    const auto commit = [](const std::string&) { return R_SUCCEEDED(core_save_commit()); };
    bool ok = save_archive::clear_dir(root, why) && R_SUCCEEDED(core_save_commit()) && save_archive::unzip_to(zip, root, commit, nullptr, why) &&
              R_SUCCEEDED(core_save_commit());
    if (!ok) {
        std::string again;
        *put_back = save_archive::clear_dir(root, &again) && save_archive::unzip_to(kept, root, commit, nullptr, &again) &&
                    R_SUCCEEDED(core_save_commit());
    }
    core_save_unmount();
    return ok;
}

void restore_picked(uint64_t title_id, const std::string& name, const CoreSaveUser& user, const local_backups::Copy& copy)
{
    const std::string dir = local_backups::dir_of(paths::data_dir(), title_id);
    const std::string zip = dir + "/" + copy.name;
    const std::string kept = dir + "/" + local_backups::safety_name(ui::today_date(), ui::now_hms());
    ui::in_background("local saves restore", [title_id, name, user, zip, kept]() {
        bool        put_back = false;
        std::string why;
        const bool  ok = restore_copy(user, title_id, zip, kept, &put_back, &why);
        aurore::sync([ok, why, put_back, kept, name]() {
            if (!ok) {
                std::string before;
                if (put_back) before = "playshelf/services/save_put_back"_i18n;
                else if (paths::exists_or_pending(kept)) before = aurore::getStr("playshelf/services/save_kept_at", kept);
                ui::error("playshelf/services/save_restore_failed"_i18n, before.empty() ? why : before + "\n\n" + why);
                return;
            }
            ui::info(aurore::getStr("playshelf/services/save_restored", name, kept));
        });
    });
}
}   // namespace

void open_restore(uint64_t title_id, const std::string& name)
{
    if (ui::refuse_read_only()) return;
    CoreSaveUser users[CORE_SAVE_USERS_MAX];
    const int    count = core_save_users(users, CORE_SAVE_USERS_MAX);
    if (count < 0) {
        ui::error("playshelf/saves_local/restore_action"_i18n, "playshelf/saves_local/users_failed"_i18n);
        return;
    }
    const std::string dir = local_backups::dir_of(paths::data_dir(), title_id);
    // Only a copy of a user this console still has can be put back.
    std::vector<local_backups::Copy> shown;
    std::vector<CoreSaveUser>        owners;
    std::vector<std::string>         labels;
    for (const local_backups::Copy& c : local_backups::copies(paths::list_files(dir, ".zip"))) {
        for (int i = 0; i < count; i++) {
            if (local_backups::user_tag(users[i].uid[0], users[i].uid[1]) != c.tag) continue;
            shown.push_back(c);
            owners.push_back(users[i]);
            labels.push_back(c.date + " " + c.time + "  ·  " + users[i].name);
            break;
        }
    }
    if (shown.empty()) {
        ui::info(aurore::getStr("playshelf/saves_local/restore_none", name));
        return;
    }
    ui::pick("playshelf/saves_local/restore_pick"_i18n, labels, 0, [title_id, name, shown, owners](int i) {
        const local_backups::Copy& copy = shown[(size_t)i];
        const CoreSaveUser         user = owners[(size_t)i];
        ui::confirm_danger(aurore::getStr("playshelf/services/save_restore_body", name, user.name, copy.date + " " + copy.time),
                           "playshelf/services/save_restore"_i18n, [title_id, name, user, copy]() { restore_picked(title_id, name, user, copy); });
    });
}

void open(uint64_t title_id, const std::string& name)
{
    if (ui::refuse_read_only()) return;
    ui::in_background("local saves", [title_id, name]() {
        const Outcome o = back_up_game(title_id);
        aurore::sync([o, name]() {
            if (!o.error.empty()) {
                ui::error("playshelf/saves_local/action"_i18n, aurore::getStr("playshelf/saves_local/failed", name, o.error));
                return;
            }
            if (o.copies == 0) ui::info(aurore::getStr("playshelf/saves_local/none", name));
            else ui::info(aurore::getStr("playshelf/saves_local/done", name, o.copies));
        });
    });
}

}   // namespace local_saves_flow
