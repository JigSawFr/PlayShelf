// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/notices.hpp"

#include <map>
#include <memory>

#include <aurore.hpp>

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

void tell()
{
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

int listen(std::function<void()> fn)
{
    return s_listeners.listen(std::move(fn));
}

void unlisten(int id)
{
    s_listeners.unlisten(id);
}

}   // namespace notices
