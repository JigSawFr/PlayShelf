// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/server_activity.hpp"

#include <utility>

#include <aurore/cadran/ui/cadran_list.hpp>

#include "library/servers.hpp"
#include "ui/ui.hpp"

using namespace aurore::literals;

namespace
{
const char* const CONNECTIONS[] = { "auto", "lan", "wan" };
const char* const FORMATS[] = { "auto", "api", "index" };

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

std::string format_text(library::Format f)
{
    return aurore::getStr(std::string("playshelf/library/format_values/") + FORMATS[(int)f]);
}

std::string or_none(const std::string& text)
{
    return text.empty() ? "playshelf/library/none"_i18n : text;
}
}   // namespace

ServerActivity::ServerActivity(library::Server s, bool added, std::function<void(const std::string&)> on_saved)
    : server(std::move(s)), is_new(added), saved(std::move(on_saved))
{
    this->password = library::password_of(this->server);
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
    format->registerClickAction([this](aurore::View*) {
        std::vector<std::string> labels;
        labels.reserve(3);
        for (int i = 0; i < 3; i++) labels.push_back(format_text((library::Format)i));
        ui::pick("playshelf/library/format"_i18n, labels, (int)this->server.format, [this](int i) {
            this->server.format = (library::Format)i;
            this->show();
        });
        return true;
    });
    user->registerClickAction([this](aurore::View*) {
        ui::prompt_text("playshelf/library/username"_i18n, this->server.username, 128, [this](const std::string& text) {
            this->server.username = text;
            this->show();
        });
        return true;
    });
    pass->registerClickAction([this](aurore::View*) {
        // Never shown again once typed: the keyboard opens empty.
        ui::prompt_text("playshelf/library/password"_i18n, "", 256, [this](const std::string& text) {
            this->password = text;
            this->password_changed = true;
            this->show();
        });
        return true;
    });
    pin->registerClickAction([this](aurore::View*) {
        if (this->server.pin.empty()) return true;
        ui::confirm("playshelf/library/forget_pin_body"_i18n, "playshelf/library/forget_pin"_i18n, [this]() {
            this->server.pin.clear();
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
                 { "sv_format", "list" }, { "sv_user", "user" }, { "sv_password", "key" }, { "sv_pin", "lock" },
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
    ui::set_detail(format, format_text(this->server.format));
    ui::set_detail(user, or_none(this->server.username));
    ui::set_detail(pass, this->password.empty() ? "playshelf/library/none"_i18n : "playshelf/library/password_set"_i18n);
    ui::set_detail(pin, or_none(library::pin_shown(this->server)));
}

void ServerActivity::ask_url(std::string* url)
{
    const std::string initial = url->empty() ? "https://" : *url;
    ui::prompt_text("playshelf/library/url_header"_i18n, initial, 1024, [this, url](const std::string& text) {
        std::string value = text == "https://" ? "" : text;
        while (!value.empty() && value.back() == ' ') value.pop_back();
        if (!value.empty() && (value.size() <= 8 || value.compare(0, 8, "https://") != 0)) {
            ui::error("playshelf/library/https_only"_i18n);
            return;
        }
        // Another server on this network: the key trusted was the other's.
        if (url == &this->server.lan_url && value != *url) this->server.pin.clear();
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
    if (this->password_changed && !library::set_password(this->server, this->password)) {
        ui::error("playshelf/library/seal_failed"_i18n);
        return;
    }
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
    aurore::Application::popActivity(aurore::TransitionAnimation::FADE, [done, id]() {
        if (done) done(id);
    });
}

void ServerActivity::remove()
{
    if (ui::refuse_read_only()) return;
    ui::confirm_danger(aurore::getStr("playshelf/library/delete_body", this->server.name), "playshelf/library/delete"_i18n,
                       [this]() {
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
