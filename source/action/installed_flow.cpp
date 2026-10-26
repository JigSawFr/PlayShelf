// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/installed_flow.hpp"

#include <borealis.hpp>
#include <map>
#include <mutex>

#include "action/library_flow.hpp"
#include "core/games.h"
#include "ui/ui.hpp"

namespace installed_flow
{
namespace
{
using Done = std::function<void(std::shared_ptr<const std::vector<Game>>)>;

// The UI thread's only.
std::shared_ptr<const std::vector<Game>> s_games;
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

// On the worker thread.
Control control_of(uint64_t id, uint32_t running)
{
    {
        const std::lock_guard<std::mutex> g(s_control_lock);
        const auto                        it = s_controls.find(id);
        if (it != s_controls.end() && it->second.running == running) return it->second;
    }
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
}   // namespace

void read(std::function<void(std::shared_ptr<const std::vector<Game>>)> done)
{
    const bool busy = !s_waiting.empty();
    s_waiting.push_back(std::move(done));
    if (busy) return;
    library_flow::reread_installed([]() {
        const auto installed = library_flow::installed();
        ui::in_background("installed games", [installed]() {
            auto out = std::make_shared<std::vector<Game>>();
            for (library::InstalledGame& g : library::installed_games(*installed)) {
                Game game;
                const Control c = control_of(g.id, library::running_version(g));
                game.name = c.name;
                game.version = c.version;
                game.icon = c.icon;
                game.game = std::move(g);
                out->push_back(std::move(game));
            }
            brls::sync([out]() {
                s_games = out;
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
        brls::sync([id, name, done, rc]() {
            if (R_FAILED(rc)) {
                brls::Logger::warning("library: {:016X} could not be deleted: 0x{:X}", id, rc);
                ui::notify(brls::getStr("playshelf/installed/delete_failed", name), ui::Toast::Refused);
            } else {
                brls::Logger::info("library: {:016X} deleted", id);
                ui::notify(brls::getStr("playshelf/installed/deleted", name), ui::Toast::Done);
            }
            read(done);
        });
    });
}

std::shared_ptr<const std::vector<Game>> games()
{
    return s_games;
}

}   // namespace installed_flow
