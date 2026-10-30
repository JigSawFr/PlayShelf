// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/installed_flow.hpp"

#include <algorithm>

#include <aurore.hpp>
#include <map>
#include <mutex>

#include "action/library_flow.hpp"
#include "bl_cache.h"
#include "core/games.h"
#include "core/play.h"
#include "ui/ui.hpp"
#include "util/paths.hpp"

namespace installed_flow
{
namespace
{
using Done = std::function<void(std::shared_ptr<const std::vector<Game>>)>;

// The UI thread's only.
std::shared_ptr<const std::vector<Game>> s_games;
Space                                    s_space;
std::vector<Done>                        s_waiting;   // a read under way: told at its end

// The names and icons read this run, by title id (the worker's, guarded),
// with the version the game ran then: an update installed since, read again
// (its version shown, its name maybe).
struct Control
{
    std::string                                 name, version;
    std::shared_ptr<const std::vector<uint8_t>> icon;
    uint32_t                                    running = 0;
};
std::mutex                    s_control_lock;
std::map<uint64_t, Control>   s_controls;

// On the worker thread. `fresh`: read from ns now (not this run's earlier read).
Control control_of(uint64_t id, uint32_t running, bool* fresh)
{
    *fresh = false;
    {
        const std::lock_guard<std::mutex> g(s_control_lock);
        const auto                        it = s_controls.find(id);
        if (it != s_controls.end() && it->second.running == running) return it->second;
    }
    *fresh = true;
    Control              c;
    c.running = running;
    char                 name[0x201] = "", version[0x20] = "";
    std::vector<uint8_t> icon(CORE_GAME_ICON_MAX);
    size_t               size = 0;
    if (core_game_control(id, name, sizeof name, version, sizeof version, icon.data(), &size)) {
        c.name = name;
        c.version = version;
        if (size) {
            icon.resize(size);
            c.icon = std::make_shared<const std::vector<uint8_t>>(std::move(icon));
        }
    }
    const std::lock_guard<std::mutex> g(s_control_lock);
    s_controls[id] = c;
    return c;
}

// The names and icons read from ns now, kept for the other Boréale
// homebrews (boreale-link's bl_cache, config/boreale/cache/): PlayGuard
// shows them without asking ns again, and its agent publishes the icons.
// On the worker thread.
void share(const std::vector<std::pair<uint64_t, Control>>& read)
{
    if (read.empty()) return;
    BlCache cache;
    if (!bl_cache_open(&cache, paths::sd_root().c_str(), core_game_language())) return;
    std::vector<uint64_t>    ids;
    std::vector<const char*> names;
    for (const auto& [id, c] : read) {
        if (c.icon) bl_cache_icon_put(&cache, id, c.icon->data(), c.icon->size());
        if (!c.name.empty()) {
            ids.push_back(id);
            names.push_back(c.name.c_str());
        }
    }
    if (!ids.empty()) bl_cache_names_put(&cache, ids.data(), names.data(), ids.size());
}
}   // namespace

void read(std::function<void(std::shared_ptr<const std::vector<Game>>)> done)
{
    const bool busy = !s_waiting.empty();
    s_waiting.push_back(std::move(done));
    if (busy) return;
    library_flow::reread_installed([]() {
        const auto installed = library_flow::installed();
        ui::in_background("installed games", [installed]() {
            auto       out = std::make_shared<std::vector<Game>>();
            const bool play = core_play_open();
            std::vector<std::pair<uint64_t, Control>> read;
            for (library::InstalledGame& g : library::installed_games(*installed)) {
                Game game;
                bool          fresh = false;
                const Control c = control_of(g.id, library::running_version(g), &fresh);
                if (fresh) read.emplace_back(g.id, c);
                game.name = c.name;
                game.version = c.version;
                game.icon = c.icon;
                u64 size = 0;
                if (core_game_size(g.id, &size)) game.size = size;
                CorePlay stats;
                if (play && core_play_stats(g.id, &stats)) {
                    game.play_s = stats.total_s;
                    game.launches = stats.launches;
                    game.last_played = stats.last_played;
                }
                game.game = std::move(g);
                out->push_back(std::move(game));
            }
            if (play) core_play_close();
            share(read);
            Space room;
            room.known = core_sd_space(&room.total, &room.free);
            aurore::sync([out, room]() {
                s_games = out;
                s_space = room;
                const std::vector<Done> waiting = std::move(s_waiting);
                s_waiting.clear();
                for (const Done& d : waiting)
                    if (d) d(out);
            });
        });
    });
}

void remove(uint64_t id, const std::string& name, const std::function<void(std::shared_ptr<const std::vector<Game>>)>& done)
{
    ui::in_background("delete game", [id, name, done]() {
        const Result rc = core_game_delete(id);
        aurore::sync([id, name, done, rc]() {
            if (R_FAILED(rc)) {
                aurore::Logger::warning("library: {:016X} could not be deleted: 0x{:X}", id, rc);
                ui::notify(aurore::getStr("playshelf/installed/delete_failed", name), ui::Toast::Refused);
            } else {
                aurore::Logger::info("library: {:016X} deleted", id);
                ui::notify(aurore::getStr("playshelf/installed/deleted", name), ui::Toast::Done);
            }
            read(done);
        });
    });
}

std::string play_text(uint64_t seconds)
{
    if (seconds >= 3600) return aurore::getStr("playshelf/home/played_hours", seconds / 3600);
    return aurore::getStr("playshelf/home/played_minutes", std::max<uint64_t>(1, seconds / 60));
}

std::shared_ptr<const std::vector<Game>> games()
{
    return s_games;
}

Space space()
{
    return s_space;
}

}   // namespace installed_flow
