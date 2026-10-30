// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/library_flow.hpp"

#include <algorithm>
#include <atomic>
#include <format>
#include <aurore.hpp>
#include <map>

#include "action/download_flow.hpp"
#include "action/popular_flow.hpp"
#include "action/titledb_flow.hpp"
#include "activity/server_activity.hpp"
#include "install/console_core.hpp"
#include "library/http_transport.hpp"
#include "library/installed.hpp"
#include "library/servers.hpp"
#include "ui/ui.hpp"

using namespace aurore::literals;

namespace library_flow
{
namespace
{
// The UI thread's only.
std::map<std::string, std::shared_ptr<const Loaded>> s_loaded;
std::map<std::string, double>                        s_speed;   // bytes a second, by server: what its downloads ran at
bool                                                 s_loading = false;
std::vector<std::function<void(std::shared_ptr<const Loaded>, std::string)>> s_waiting;   // the fetch's `done`s
std::shared_ptr<const Installed>                     s_installed = std::make_shared<Installed>();

// On a worker thread: what is installed, read now.
std::shared_ptr<const Installed> read_installed()
{
    auto got = install::installed_versions();
    if (!got) {
        aurore::Logger::warning("library: what is installed could not be read: {}", got.error());
        return std::make_shared<Installed>();
    }
    return std::make_shared<Installed>(std::move(*got));
}

using Done = std::function<void(std::shared_ptr<const Loaded>, std::string)>;

// The pin kept with the server in servers.json, and in `server`.
bool trust(library::Server& server, const std::string& pin)
{
    std::vector<library::Server> all = library::load_servers();
    for (library::Server& s : all)
        if (s.id == server.id) s.pin = pin;
    std::string why;
    if (!library::save_servers(all, &why)) {
        ui::error("playshelf/library/save_failed"_i18n, why);
        return false;
    }
    server.pin = pin;
    return true;
}

void fetched(const library::Server& server, const std::shared_ptr<Loaded>& l, std::shared_ptr<const Installed> now, const Done& done)
{
    s_loading = false;
    s_installed = std::move(now);
    const library::Fetch& f = l->fetch;
    if (f.status == library::FetchStatus::Ok) {
        s_loaded[server.id] = l;
        done(l, "");
        return;
    }
    if (f.status == library::FetchStatus::UnknownCertificate) {
        const std::string body = aurore::getStr("playshelf/library/trust_body", server.name, f.endpoint, f.offered_short);
        const std::string pin = f.offered_pin;
        ui::confirm(
            body, "playshelf/library/trust"_i18n,
            [copy = server, pin, done]() mutable {
                if (ui::refuse_read_only() || !trust(copy, pin)) {
                    done(nullptr, "");
                    return;
                }
                load(copy, done);
            },
            [done]() { done(nullptr, ""); });
        return;
    }
    aurore::Logger::warning("library: {} could not be read: {}", server.name, f.why);
    done(nullptr, f.why);
}

void open_editor(const library::Server& server, bool is_new, const std::function<void(const std::string&)>& saved)
{
    aurore::Application::pushActivity(new ServerActivity(server, is_new, [saved](const std::string& id) {
        forget(id);
        if (saved) saved(id);
    }));
}
}   // namespace

std::vector<Source> sources(uint64_t title_id)
{
    std::vector<Source> out;
    if (!title_id) return out;
    for (const library::Server& s : library::load_servers()) {
        const auto l = loaded(s.id);
        if (!l) continue;
        const long at = library::title_of(l->titles, title_id);
        if (at < 0) continue;
        const library::Title& t = l->titles[(size_t)at];
        Source src;
        src.server_id = s.id;
        src.server = s.name;
        src.base = t.base >= 0;
        src.updates = t.updates.size();
        src.dlcs = t.dlcs.size();
        if (!t.updates.empty()) src.newest = l->fetch.catalog.items[t.updates.front()].version;
        std::vector<std::string> parts;
        if (src.base) parts.push_back("playshelf/game/source_base"_i18n);
        if (src.updates == 1) parts.push_back(aurore::getStr("playshelf/game/source_update_one", src.newest));
        else if (src.updates) parts.push_back(aurore::getStr("playshelf/game/source_update", src.updates, src.newest));
        if (src.dlcs == 1) parts.push_back("playshelf/game/source_dlc_one"_i18n);
        else if (src.dlcs) parts.push_back(aurore::getStr("playshelf/game/source_dlc", src.dlcs));
        for (size_t i = 0; i < parts.size(); i++) src.what += (i ? " · " : "") + parts[i];
        out.push_back(std::move(src));
    }
    return out;
}

std::string size_text(uint64_t bytes)
{
    const double mb = (double)bytes / (1024.0 * 1024.0);
    // A decimal while the figure is small: "0,1 MB", "4,2 GB", "734 MB".
    std::string text = mb >= 1024.0 ? std::format("{:.1f}", mb / 1024.0) : mb < 10.0 ? std::format("{:.1f}", mb) : std::format("{:.0f}", mb);
    const size_t dot = text.find('.');
    if (dot != std::string::npos) text.replace(dot, 1, "playshelf/games/decimal_mark"_i18n);
    return aurore::getStr(mb >= 1024.0 ? "playshelf/library/size_gb" : "playshelf/library/size_mb", text);
}

std::shared_ptr<const Installed> installed()
{
    return s_installed;
}

std::shared_ptr<const Loaded> loaded(const std::string& server_id)
{
    const auto it = s_loaded.find(server_id);
    return it == s_loaded.end() ? nullptr : it->second;
}

void forget(const std::string& server_id)
{
    s_loaded.erase(server_id);
}

bool loading()
{
    return s_loading;
}

void load(const library::Server& server, const Done& done)
{
    s_waiting.push_back(done);
    if (s_loading) return;
    s_loading = true;
    // Each one asked meanwhile told too.
    const Done all = [](const std::shared_ptr<const Loaded>& l, const std::string& why) {
        auto waiting = std::move(s_waiting);
        s_waiting.clear();
        for (const Done& d : waiting) d(l, why);
    };
    const std::string password = library::password_of(server);
    const auto        atlas = popular_flow::signals();
    ui::in_background("library", [server, password, all, atlas]() {
        library::HttpTransport transport;
        auto                   l = std::make_shared<Loaded>();
        l->server = server;
        l->fetch = library::fetch(server, password, transport);
        if (l->fetch.status == library::FetchStatus::Ok) {
            l->titles = library::group_titles(l->fetch.catalog);
            // A game the server names only by its file: the title
            // database's name, when it has one, else titledb's as Atlas
            // tells it (games.json).
            for (library::Title& t : l->titles) {
                if (!t.id) continue;
                const auto known = l->fetch.catalog.titles.find(t.id);
                if (known != l->fetch.catalog.titles.end() && !known->second.name.empty()) continue;
                if (const auto local = titledb_flow::title(t.id); local && !local->name.empty()) t.name = local->name;
                else if (const library::GameSignals* g = atlas->find(t.id); g && !g->name.empty()) t.name = g->name;
            }
        }
        const auto now = read_installed();
        aurore::sync([server, l, now, all]() { fetched(server, l, now, all); });
    });
}

void add(const std::function<void(const std::string&)>& saved)
{
    if (ui::refuse_read_only()) return;
    library::Server s;
    s.id = library::new_server_id();
    open_editor(s, true, saved);
}

void edit(const library::Server& server, const std::function<void(const std::string&)>& saved)
{
    open_editor(server, false, saved);
}

void discover(const std::function<void(const std::string&)>& saved)
{
    if (ui::refuse_read_only()) return;
    auto          cancel = std::make_shared<std::atomic<bool>>(false);
    aurore::Dialog* dialog = ui::dialog("playshelf/library/discovering"_i18n);
    ui::add_cancel(dialog, [cancel]() { *cancel = true; });
    dialog->open();
    ui::in_background("discovery", [cancel, dialog, saved]() {
        std::string why;
        const auto  found = library::discover(cancel.get(), &why);
        aurore::sync([cancel, dialog, saved, found, why]() {
            if (*cancel) return;   // the dialog is closed already
            dialog->close();
            if (found.empty()) {
                if (!why.empty()) ui::error("playshelf/library/discover_failed"_i18n, why);
                else ui::info("playshelf/library/discover_none"_i18n);
                return;
            }
            std::vector<std::string> names, details;
            for (const library::Discovered& d : found) {
                names.push_back(d.name.empty() ? d.address : d.name);
                details.push_back(d.address + ":" + std::to_string(d.port));
            }
            ui::pick("playshelf/library/discover_pick"_i18n, names, 0, [found, saved](int i) {
                const library::Discovered& d = found[(size_t)i];
                library::Server            s;
                s.id = library::new_server_id();
                s.name = d.name;
                s.lan_url = d.url();
                if (d.remote.starts_with("https://")) s.wan_url = d.remote;
                open_editor(s, true, saved);
            }, false, details);
        });
    });
}

void reread_installed(const std::function<void()>& after)
{
    ui::in_background("installed", [after]() {
        const auto now = read_installed();
        aurore::sync([now, after]() {
            s_installed = now;
            if (after) after();
        });
    });
}

void note_speed(const std::string& server_id, double bytes_per_second)
{
    if (server_id.empty() || bytes_per_second <= 0) return;
    double& known = s_speed[server_id];
    known = known > 0 ? known * 0.8 + bytes_per_second * 0.2 : bytes_per_second;
}

namespace
{
// How good a server is to get a package from: one on this network first,
// then the fastest its downloads ran (an unknown one as a slow one).
double rank_of(const Loaded& l)
{
    const auto it = s_speed.find(l.server.id);
    return (l.fetch.on_lan ? 1e12 : 0) + (it == s_speed.end() ? 0 : it->second);
}

// The catalogue to get `l`'s item `i` from: another server with the same
// package (title id, version, kind) when it ranks better, else `l`.
std::pair<const Loaded*, size_t> best_source(const Loaded& l, size_t i)
{
    const library::Item&             it = l.fetch.catalog.items[i];
    std::pair<const Loaded*, size_t> best{ &l, i };
    double                           best_rank = rank_of(l);
    if (!it.title_id || !it.has_version) return best;
    for (const auto& [id, other] : s_loaded) {
        if (!other || id == l.server.id) continue;
        const double rank = rank_of(*other);
        if (rank <= best_rank) continue;
        const auto& items = other->fetch.catalog.items;
        for (size_t k = 0; k < items.size(); k++)
            if (items[k].title_id == it.title_id && items[k].has_version && items[k].version == it.version &&
                items[k].kind == it.kind) {
                best = { other.get(), k };
                best_rank = rank;
                break;
            }
    }
    return best;
}
}   // namespace

size_t install(const Loaded& from, const std::vector<size_t>& items)
{
    if (ui::refuse_read_only()) return 0;
    std::vector<download::Job> jobs;
    for (size_t asked : items) {
        if (asked >= from.fetch.catalog.items.size()) continue;
        // From the best server that has it (on this network, the fastest).
        const auto [source, i] = best_source(from, asked);
        const Loaded& l = *source;
        if (&l != &from)
            aurore::Logger::info("library: {} from {} rather than {}", l.fetch.catalog.items[i].name, l.server.name, from.server.name);
        const library::Item& it = l.fetch.catalog.items[i];
        download::Job        j;
        // Its game's name, and which item of it ("Update v131072", an
        // add-on's own name).
        j.name = library::clean_name(it.name);
        j.label = j.name;
        for (const library::Title& t : l.titles) {
            const bool update = std::ranges::contains(t.updates, i);
            const bool dlc = std::ranges::contains(t.dlcs, i);
            if (!update && !dlc && (long)i != t.base) continue;
            j.game = t.name;
            j.game_id = t.id;
            j.icon_url = t.icon_url;
            j.label = update ? aurore::getStr("playshelf/library/update", it.version)
                    : dlc    ? library::clean_name(it.name)
                             : "playshelf/library/base"_i18n;
            j.name = update || dlc ? t.name + " · " + j.label : t.name;
        }
        j.url = it.url;
        j.server_id = l.server.id;
        j.title_id = it.title_id;
        if (it.has_version) j.version = it.version;
        j.size = it.size;
        jobs.push_back(std::move(j));
    }
    return download_flow::add(std::move(jobs));
}

GameLines game_lines(const std::shared_ptr<const Loaded>& data, size_t index, const std::vector<ui::Choice>& more)
{
    GameLines out;
    if (!data || index >= data->titles.size()) return out;
    const library::Title&             t = data->titles[index];
    const std::vector<library::Item>& items = data->fetch.catalog.items;
    using Choice = ui::Choice;
    const auto have = installed();
    auto       add = [data](const std::vector<size_t>& chosen) {
        return [data, chosen]() {
            const size_t added = install(*data, chosen);
            if (added) ui::notify(aurore::getStr("playshelf/downloads/added", added));
            else if (!chosen.empty()) ui::notify("playshelf/downloads/already"_i18n);
        };
    };
    const library::TitleState st = library::state_of(t, data->fetch.catalog, *have);
    // Its line's end: its size, or that the console has it.
    auto line = [&](std::vector<Choice>& to, size_t i, const char* icon, const std::string& label) {
        const bool has = library::is_installed(items[i], *have);
        to.emplace_back(label, add({ i }), icon, has ? "playshelf/library/installed"_i18n : size_text(items[i].size), "",
                        has ? Choice::Tone::ON : Choice::Tone::PLAIN);
        return has;
    };

    // Everything at once, what the console does not have of it: the base
    // game, its newest update (when newer than the one installed), its
    // add-ons.
    std::vector<size_t> missing, dlcs_missing;
    if (t.base >= 0 && !st.base) missing.push_back((size_t)t.base);
    const long latest = library::latest_update(t);
    const bool latest_missing = latest >= 0 && !library::is_installed(items[(size_t)latest], *have) &&
                                (!st.base || st.update_available);
    if (latest_missing) missing.push_back((size_t)latest);
    for (size_t i : t.dlcs)
        if (!library::is_installed(items[i], *have)) dlcs_missing.push_back(i);
    missing.insert(missing.end(), dlcs_missing.begin(), dlcs_missing.end());
    auto size_of = [&](const std::vector<size_t>& chosen) {
        uint64_t n = 0;
        for (size_t i : chosen) n += items[i].size;
        return n;
    };
    if (missing.size() > 1 || (st.base && !missing.empty()))
        out.main.emplace_back(st.base ? "playshelf/library/install_missing"_i18n : "playshelf/library/install_all"_i18n,
                              add(missing), "download", size_text(size_of(missing)));
    if (t.base >= 0) {
        line(out.main, (size_t)t.base, "gamepad", "playshelf/library/base"_i18n);
        out.base_size = items[(size_t)t.base].size;
    }

    // Its updates: the newest at once when the console has an older one.
    if (latest_missing && st.base)
        out.updates.emplace_back("playshelf/game/install_latest"_i18n, add({ (size_t)latest }), "download",
                                 size_text(items[(size_t)latest].size));
    for (size_t i : t.updates) {
        // Its number (the version's high half) before the version itself.
        line(out.updates, i, "refresh", aurore::getStr("playshelf/game/update_line", items[i].version >> 16, items[i].version));
        out.updates_size += items[i].size;
    }
    out.update_count = t.updates.size();
    if (latest >= 0) out.latest_version = items[(size_t)latest].version;

    // Its add-ons: the missing ones at once, when there are several.
    if (dlcs_missing.size() > 1)
        out.dlcs.emplace_back("playshelf/game/install_missing_dlcs"_i18n, add(dlcs_missing), "download",
                              size_text(size_of(dlcs_missing)));
    for (size_t i : t.dlcs) {
        if (line(out.dlcs, i, "plus", library::clean_name(items[i].name))) out.dlcs_installed++;
        out.dlcs_size += items[i].size;
    }
    out.dlc_count = t.dlcs.size();

    out.more = more;
    out.installed = st.base;
    out.installed_version = st.base ? st.version : 0;
    out.facts = t.id ? std::format("{:016X} · {}", t.id, size_text(t.size)) : size_text(t.size);
    if (st.base) out.facts += " · " + aurore::getStr("playshelf/library/installed_version", st.version);
    return out;
}

}   // namespace library_flow
