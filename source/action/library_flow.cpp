// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/library_flow.hpp"

#include <algorithm>
#include <atomic>
#include <borealis.hpp>
#include <map>

#include <fmt/format.h>

#include "action/download_flow.hpp"
#include "activity/server_activity.hpp"
#include "install/console_core.hpp"
#include "library/http_transport.hpp"
#include "library/servers.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

namespace library_flow
{
namespace
{
// The UI thread's only.
std::map<std::string, std::shared_ptr<const Loaded>> s_loaded;
bool                                                 s_loading = false;
std::shared_ptr<const Installed>                     s_installed = std::make_shared<Installed>();

// On a worker thread: what is installed, read now.
std::shared_ptr<const Installed> read_installed()
{
    std::string why;
    auto        out = std::make_shared<Installed>(install::installed_versions(&why));
    if (!why.empty()) brls::Logger::warning("library: what is installed could not be read: {}", why);
    return out;
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
        const std::string body = brls::getStr("playshelf/library/trust_body", server.name, f.endpoint, f.offered_short);
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
    brls::Logger::warning("library: {} could not be read: {}", server.name, f.why);
    done(nullptr, f.why);
}

void open_editor(const library::Server& server, bool is_new, const std::function<void(const std::string&)>& saved)
{
    brls::Application::pushActivity(new ServerActivity(server, is_new, [saved](const std::string& id) {
        forget(id);
        if (saved) saved(id);
    }));
}
}   // namespace

std::string size_text(uint64_t bytes)
{
    const double mb = (double)bytes / (1024.0 * 1024.0);
    // A decimal while the figure is small: "0,1 MB", "4,2 GB", "734 MB".
    std::string text = mb >= 1024.0 ? fmt::format("{:.1f}", mb / 1024.0) : mb < 10.0 ? fmt::format("{:.1f}", mb) : fmt::format("{:.0f}", mb);
    const size_t dot = text.find('.');
    if (dot != std::string::npos) text.replace(dot, 1, "playshelf/games/decimal_mark"_i18n);
    return brls::getStr(mb >= 1024.0 ? "playshelf/library/size_gb" : "playshelf/library/size_mb", text);
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
    if (s_loading) return;
    s_loading = true;
    const std::string password = library::password_of(server);
    ui::in_background("library", [server, password, done]() {
        library::HttpTransport transport;
        auto                   l = std::make_shared<Loaded>();
        l->server = server;
        l->fetch = library::fetch(server, password, transport);
        if (l->fetch.status == library::FetchStatus::Ok) l->titles = library::group_titles(l->fetch.catalog);
        const auto now = read_installed();
        brls::sync([server, l, now, done]() { fetched(server, l, now, done); });
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
    brls::Dialog* dialog = ui::dialog("playshelf/library/discovering"_i18n);
    ui::add_cancel(dialog, [cancel]() { *cancel = true; });
    dialog->open();
    ui::in_background("discovery", [cancel, dialog, saved]() {
        std::string why;
        const auto  found = library::discover(cancel.get(), &why);
        brls::sync([cancel, dialog, saved, found, why]() {
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
                if (d.remote.compare(0, 8, "https://") == 0) s.wan_url = d.remote;
                open_editor(s, true, saved);
            }, false, details);
        });
    });
}

void reread_installed(const std::function<void()>& after)
{
    ui::in_background("installed", [after]() {
        const auto now = read_installed();
        brls::sync([now, after]() {
            s_installed = now;
            if (after) after();
        });
    });
}

size_t install(const Loaded& l, const std::vector<size_t>& items)
{
    if (ui::refuse_read_only()) return 0;
    std::vector<download::Job> jobs;
    for (size_t i : items) {
        if (i >= l.fetch.catalog.items.size()) continue;
        const library::Item& it = l.fetch.catalog.items[i];
        download::Job        j;
        // Its game's name, and which item of it ("Update v131072", an
        // add-on's own name).
        j.name = library::clean_name(it.name);
        for (const library::Title& t : l.titles)
            if (std::find(t.updates.begin(), t.updates.end(), i) != t.updates.end())
                j.name = t.name + " · " + brls::getStr("playshelf/library/update", it.version);
            else if ((long)i == t.base)
                j.name = t.name;
        j.url = it.url;
        j.server_id = l.server.id;
        j.title_id = it.title_id;
        j.size = it.size;
        jobs.push_back(std::move(j));
    }
    return download_flow::add(std::move(jobs));
}

}   // namespace library_flow
