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
