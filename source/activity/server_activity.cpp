// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/server_activity.hpp"

#include <utility>

#include <aurore/cadran/ui/cadran_list.hpp>

#include "action/pairing_flow.hpp"
#include "lantern_app/credential_store.hpp"
#include "library/servers.hpp"
#include "ui/ui.hpp"

using namespace aurore::literals;

namespace
{
const char* const CONNECTIONS[] = { "auto", "lan", "wan" };

library::Connection connection_at(int i)
{
    return i == 1 ? library::Connection::Lan : i == 2 ? library::Connection::Wan : library::Connection::Auto;
}

int connection_index(library::Connection c)
{
    return c == library::Connection::Lan ? 1 : c == library::Connection::Wan ? 2 : 0;
}

std::string connection_text(library::Connection c)
{
    return aurore::getStr(std::string("playshelf/library/connection_values/") + CONNECTIONS[connection_index(c)]);
}

std::string or_none(const std::string& text)
{
    return text.empty() ? "playshelf/library/none"_i18n : text;
}
}   // namespace

ServerActivity::ServerActivity(library::Server s, bool added, std::function<void(const std::string&)> on_saved)
    : server(std::move(s)), is_new(added), saved(std::move(on_saved))
{
}

void ServerActivity::onContentAvailable()
{
    if (auto* applet = dynamic_cast<aurore::AppletFrame*>(this->getContentView()))
        applet->setTitle(this->is_new ? "playshelf/library/add_server"_i18n : "playshelf/library/edit_server"_i18n);

    name->registerClickAction([this](aurore::View*) {
        ui::prompt_text("playshelf/library/name"_i18n, this->server.name, 64, [this](const std::string& text) {
            this->server.name = text;
            this->show();
        });
        return true;
    });
    lan->registerClickAction([this](aurore::View*) {
        this->ask_url(&this->server.lan_url);
        return true;
    });
    wan->registerClickAction([this](aurore::View*) {
        this->ask_url(&this->server.wan_url);
        return true;
    });
    connection->registerClickAction([this](aurore::View*) {
        std::vector<std::string> labels;
        labels.reserve(3);
        for (int i = 0; i < 3; i++) labels.push_back(connection_text(connection_at(i)));
        ui::pick("playshelf/library/connection"_i18n, labels, connection_index(this->server.connection), [this](int i) {
            this->server.connection = connection_at(i);
            this->show();
        });
        return true;
    });
    pairing->registerClickAction([this](aurore::View*) {
        if (this->is_new) {
            ui::info("playshelf/pairing/save_first"_i18n);
            return true;
        }
        if (this->server.uid.empty()) {
            pairing_flow::start(this->server, [this](bool paired) {
                if (!paired) return;
                for (const library::Server& s : library::load_servers())
                    if (s.id == this->server.id) this->server = s;
                this->show();
            });
            return true;
        }
        ui::confirm("playshelf/pairing/forget_body"_i18n, "playshelf/pairing/forget"_i18n, [this]() {
            pairing_flow::unpair(this->server);
            this->server.uid.clear();
            this->show();
        });
        return true;
    });
    save_cell->registerClickAction([this](aurore::View*) {
        this->save();
        return true;
    });
    delete_cell->registerClickAction([this](aurore::View*) {
        this->remove();
        return true;
    });

    cadran::List list(this->getContentView(), true);
    list.apply({ { "sv_name", "tag" }, { "sv_lan", "home" }, { "sv_wan", "globe" }, { "sv_connection", "server" },
                 { "sv_pairing", "key" },
                 { "sv_save", "save" }, { "sv_delete", "trash" } });
    ui::set_visible(delete_cell, !this->is_new);
    this->show();
}

void ServerActivity::show()
{
    ui::set_detail(name, or_none(this->server.name));
    ui::set_detail(lan, or_none(this->server.lan_url));
    ui::set_detail(wan, or_none(this->server.wan_url));
    ui::set_detail(connection, connection_text(this->server.connection));
    ui::set_detail(pairing, this->server.uid.empty() ? "playshelf/pairing/not_paired"_i18n : "playshelf/pairing/paired_state"_i18n);
}

void ServerActivity::ask_url(std::string* url)
{
    const std::string initial = url->empty() ? "https://" : *url;
    ui::prompt_text("playshelf/library/url_header"_i18n, initial, 1024, [this, url](const std::string& text) {
        std::string value = text == "https://" ? "" : text;
        while (!value.empty() && value.back() == ' ') value.pop_back();
        if (!value.empty() && (value.size() <= 8 || !value.starts_with("https://"))) {
            ui::error("playshelf/library/https_only"_i18n);
            return;
        }
        // Another server at this address: the pairing was with the other.
        if (value != *url && !this->server.uid.empty()) {
            pairing_flow::unpair(this->server);
            this->server.uid.clear();
        }
        *url = value;
        this->show();
    });
}

void ServerActivity::save()
{
    if (ui::refuse_read_only()) return;
    if (!library::check_server(this->server).empty()) {
        ui::error("playshelf/library/need_address"_i18n);
        return;
    }
    if (this->server.name.empty()) this->server.name = !this->server.lan_url.empty() ? this->server.lan_url : this->server.wan_url;
    std::vector<library::Server> all = library::load_servers();
    bool                         found = false;
    for (library::Server& s : all)
        if (s.id == this->server.id) {
            s = this->server;
            found = true;
        }
    if (!found) {
        if (all.size() >= library::MAX_SERVERS) {
            ui::error("playshelf/library/too_many"_i18n);
            return;
        }
        all.push_back(this->server);
    }
    std::string why;
    if (!library::save_servers(all, &why)) {
        ui::error("playshelf/library/save_failed"_i18n, why);
        return;
    }
    const std::string id = this->server.id;
    auto              done = this->saved;
    const library::Server paired = this->server;
    aurore::Application::popActivity(aurore::TransitionAnimation::FADE, [done, id, paired]() {
        // A server not paired yet is, right away: the person is here.
        if (paired.uid.empty()) pairing_flow::start(paired, [done, id](bool) { if (done) done(id); });
        else if (done) done(id);
    });
}

void ServerActivity::remove()
{
    if (ui::refuse_read_only()) return;
    ui::confirm_danger(aurore::getStr("playshelf/library/delete_body", this->server.name), "playshelf/library/delete"_i18n,
                       [this]() {
                           pairing_flow::unpair(this->server);
                           std::vector<library::Server> all = library::load_servers();
                           std::vector<library::Server> kept;
                           for (const library::Server& s : all)
                               if (s.id != this->server.id) kept.push_back(s);
                           std::string why;
                           if (!library::save_servers(kept, &why)) {
                               ui::error("playshelf/library/save_failed"_i18n, why);
                               return;
                           }
                           auto done = this->saved;
                           aurore::Application::popActivity(aurore::TransitionAnimation::FADE, [done]() {
                               if (done) done("");
                           });
                       });
}
