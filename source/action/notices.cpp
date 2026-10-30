// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/notices.hpp"

#include <ctime>
#include <map>
#include <memory>

#include <aurore.hpp>

#include "action/shared_bell_logic.hpp"
#include "app.hpp"
#include "ui/app_hooks.hpp"
#include "ui/ui.hpp"
#include "util/listeners.hpp"
#include "util/paths.hpp"

namespace notices
{

namespace
{
notice_logic::Store                          s_store;
std::map<std::string, Entry>                 s_actions;   // each entry's button, by id
Listeners                                    s_listeners;
bool                                         s_newer = false;   // notices_read.txt is a newer build's: never written over

std::string read_file_path()
{
    return paths::data_dir() + "/notices_read.txt";
}

bool s_published = false;   // publish() ran: the bell is written at each change from then on

// `text` to `file` off the UI thread, made there from what is on the SD
// card (`make`: "" when nothing changed).
void write_shared(const char* what, const std::string& file, std::function<std::string(const std::string&)> make)
{
    ui::write_later(what, [file, make]() {
        std::string before;
        paths::read_file(file, before);
        const std::string text = make(before);
        if (text.empty()) return;
        std::string why = "cannot make its folder";
        if (!paths::ensure_dir(file.substr(0, file.rfind('/'))) || !paths::atomic_write(file, text, &why))
            aurore::Logger::warning("{} not saved: {}", file, why);
    }, file);
}

// The bell as the other Boréale homebrews read it (action/shared_bell_logic).
void write_shared_bell()
{
    std::vector<shared_bell_logic::Entry> entries;
    for (const notice_logic::Item& i : s_store.visible())
        entries.push_back({ i.id, i.level == Level::Action, i.title, i.detail, shared_bell_logic::route_of(i.id) });
    const std::string lang = aurore::Application::getLocale(), nro = app::self_path();
    write_shared("shared bell", shared_bell_logic::file(paths::sd_root()), [entries, lang, nro](const std::string& before) {
        return shared_bell_logic::text(entries, lang, nro, before, (int64_t)std::time(nullptr));
    });
}

void tell()
{
    if (s_published) write_shared_bell();
    s_listeners.notify();
}

// What was put away, written off the UI thread.
void save_read()
{
    if (s_newer) return;
    ui::save_later("notices", read_file_path(), notice_logic::keys_text(s_store.read_keys()));
}
}   // namespace

void start()
{
    std::string text;
    if (paths::read_file(read_file_path(), text)) {
        s_newer = notice_logic::keys_newer(text);
        if (s_newer) aurore::Logger::warning("notices: {} is from a newer PlayShelf, left as it is", read_file_path());
        s_store.set_read(notice_logic::keys_of(text));
    }

    ui::app_hooks::Hooks& a = ui::app_hooks::get();
    a.notice_set = [](const std::string& id, const std::string& title, const std::string& body, const std::string& button,
                      std::function<void()> action) {
        set("kit." + id, Level::Info, title, body, action ? button : "", std::move(action));
    };
    a.notice_clear = [](const std::string& id) { clear("kit." + id); };
}

void set(const std::string& id, Level level, const std::string& title, const std::string& detail,
         const std::string& action_label, std::function<void()> action, const std::string& key)
{
    s_actions[id] = Entry{ {}, action_label, std::move(action) };
    if (s_store.set(id, level, title, detail, key)) tell();
}

void clear(const std::string& id)
{
    s_actions.erase(id);
    if (s_store.clear(id)) tell();
}

void dismiss(const std::string& id)
{
    if (!s_store.dismiss(id)) return;
    save_read();
    tell();
}

void dismiss_all()
{
    bool any = false;
    for (const notice_logic::Item& i : s_store.visible())
        if (i.level == Level::Info) any |= s_store.dismiss(i.id);
    if (!any) return;
    save_read();
    tell();
}

std::vector<Entry> list()
{
    std::vector<Entry> out;
    for (const notice_logic::Item& i : s_store.visible()) {
        Entry e;
        e.item = i;
        auto it = s_actions.find(i.id);
        if (it != s_actions.end()) {
            e.action_label = it->second.action_label;
            e.action = it->second.action;
        }
        out.push_back(std::move(e));
    }
    return out;
}

int action_count()
{
    return s_store.action_count();
}

int info_count()
{
    return s_store.info_count();
}

void publish()
{
    s_published = true;
    const std::string version = app::version(), nro = app::self_path();
    write_shared("Boréale registry", shared_bell_logic::app_file(paths::sd_root()), [version, nro](const std::string& before) {
        return shared_bell_logic::app_text(version, nro, before);
    });
    write_shared_bell();
}

int listen(std::function<void()> fn)
{
    return s_listeners.listen(std::move(fn));
}

void unlisten(int id)
{
    s_listeners.unlisten(id);
}

}   // namespace notices
