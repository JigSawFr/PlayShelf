// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/pairing_flow.hpp"

#include <atomic>
#include <aurore.hpp>
#include <chrono>
#include <memory>
#include <thread>

#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/util/text.hpp>

#include "kit_text.hpp"
#include "lantern_app/platform.hpp"
#include "library/servers.hpp"
#include "ui/ui.hpp"

using namespace aurore::literals;

namespace pairing_flow
{
namespace
{
bool s_busy = false;   // one pairing at a time
constexpr int GIVE_UP_SECONDS = 15 * 60;

struct Outcome
{
    bool                              paired = false;
    std::string                       why;
    std::string                       uid, name;
};

// The uid (and the name, when none was given) kept in servers.json.
bool remember(const std::string& id, const std::string& uid, const std::string& name)
{
    std::vector<library::Server> all = library::load_servers();
    for (library::Server& s : all)
        if (s.id == id) {
            s.uid = uid;
            if (s.name.empty()) s.name = name;
        }
    std::string why;
    if (!library::save_servers(all, &why)) {
        ui::error("playshelf/library/save_failed"_i18n, why);
        return false;
    }
    return true;
}

// The addresses to try, the first that answers wins: this network's before
// the Internet's unless the server is set to one of them.
std::vector<std::pair<std::string, bool>> candidates(const library::Server& s)
{
    std::vector<std::pair<std::string, bool>> out;
    auto lan = [&] { if (!s.lan_url.empty()) out.emplace_back(s.lan_url, true); };
    auto wan = [&] { if (!s.wan_url.empty()) out.emplace_back(s.wan_url, false); };
    switch (s.connection) {
        case library::Connection::Lan: lan(); if (out.empty()) wan(); break;
        case library::Connection::Wan: wan(); if (out.empty()) lan(); break;
        default: lan(); wan(); break;
    }
    return out;
}

void show_code(const library::Server& server, std::shared_ptr<lantern_app::Platform> platform,
               std::shared_ptr<lantern_app::Pairing> p, std::function<void(bool)> done)
{
    auto cancelled = std::make_shared<std::atomic<bool>>(false);

    auto* row = new aurore::Box(aurore::Axis::ROW);
    row->setAlignItems(aurore::AlignItems::CENTER);
    row->setJustifyContent(aurore::JustifyContent::CENTER);
    aurore::Label* code = cadran::kit::display(p->code, 44);
    code->setMarginRight(32);
    row->addView(code);
    row->addView(cadran::kit::qr(p->verify_complete.empty() ? p->verify : p->verify_complete, 200, "",
                                 "playshelf/pairing/qr_note"_i18n, 260));
    const std::string key = p->fingerprint.empty() ? "playshelf/pairing/no_key"_i18n : p->fingerprint;
    auto* dialog = ui::dialog(aurore::getStr("playshelf/pairing/body", text::short_url(p->verify), key), row);
    ui::add_cancel(dialog, [cancelled]() { *cancelled = true; });
    dialog->open();

    const std::string id = server.id;
    ui::in_background("Lantern pairing", [platform, p, cancelled, dialog, done, id]() {
        Outcome out;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(GIVE_UP_SECONDS);
        while (!*cancelled && !ui::quitting() && std::chrono::steady_clock::now() < deadline) {
            const uint32_t wait = p->pairing.wait ? p->pairing.wait : 5;
            for (uint32_t s = 0; s < wait * 10 && !*cancelled && !ui::quitting(); s++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (*cancelled || ui::quitting()) break;
            const auto state = lantern_app::poll_pairing(*p, platform->store());
            if (!state) {
                out.why = lantern_app::describe(state.error());
                break;
            }
            if (*state) {
                out.paired = true;
                out.uid = p->uid;
                out.name = p->name;
                break;
            }
        }
        if (!out.paired && out.why.empty() && !*cancelled) out.why = "playshelf/pairing/expired"_i18n;
        aurore::sync([out, cancelled, dialog, done, id]() {
            s_busy = false;
            if (*cancelled) {
                if (done) done(false);
                return;
            }
            *cancelled = true;
            dialog->close();
            if (!out.paired) {
                ui::error(aurore::getStr("playshelf/pairing/failed", out.why));
                if (done) done(false);
                return;
            }
            if (!remember(id, out.uid, out.name)) {
                if (done) done(false);
                return;
            }
            ui::notify(aurore::getStr("playshelf/pairing/paired", out.name));
            if (done) done(true);
        });
    });
}
}   // namespace

void start(const library::Server& server, std::function<void(bool)> done)
{
    if (ui::refuse_read_only()) return;
    if (s_busy) return;
    s_busy = true;
    ui::notify("playshelf/pairing/starting"_i18n);
    ui::in_background("Lantern pairing", [server, done]() {
        auto platform = std::make_shared<lantern_app::Platform>();
        auto pairing  = std::make_shared<lantern_app::Pairing>();
        std::string why;
        bool        ok = false;
        for (const auto& [url, on_lan] : candidates(server)) {
            auto p = lantern_app::begin_pairing(url, on_lan, "PlayShelf", platform->env());
            if (p) {
                *pairing = std::move(*p);
                ok = true;
                break;
            }
            why = lantern_app::describe(p.error());
        }
        if (!ok && why.empty()) why = platform->problem().empty() ? "the server has no https:// address" : platform->problem();
        aurore::sync([ok, why, server, platform, pairing, done]() {
            if (!ok) {
                s_busy = false;
                ui::error(aurore::getStr("playshelf/pairing/failed", why));
                if (done) done(false);
                return;
            }
            show_code(server, platform, pairing, done);
        });
    });
}

void unpair(const library::Server& server)
{
    if (server.uid.empty()) return;
    lantern_app::FileCredentialStore store;
    (void)store.erase(server.uid);
    std::vector<library::Server> all = library::load_servers();
    for (library::Server& s : all)
        if (s.id == server.id) s.uid.clear();
    std::string why;
    (void)library::save_servers(all, &why);
}

}   // namespace pairing_flow
