// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/notice_logic.hpp"

#include <algorithm>
#include <cstdio>

#include "util/text_schema.hpp"

namespace notice_logic
{

namespace
{
// FNV-1a: a short print of an entry's text, not a secret.
uint32_t hash_of(const std::string& s)
{
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

bool is_key(const std::string& line)
{
    const size_t colon = line.rfind(':');
    if (colon == std::string::npos || colon == 0 || line.size() - colon != 9 || line.size() > 80) return false;
    for (size_t i = 0; i < line.size(); i++) {
        const char c = line[i];
        const bool ok = i > colon ? (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
                                  : (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == ':';
        if (!ok) return false;
    }
    return true;
}
}   // namespace

Store::Slot* Store::find(const std::string& id)
{
    for (Slot& s : this->slots)
        if (s.item.id == id) return &s;
    return nullptr;
}

const Store::Slot* Store::find(const std::string& id) const
{
    for (const Slot& s : this->slots)
        if (s.item.id == id) return &s;
    return nullptr;
}

bool Store::set(const std::string& id, Level level, const std::string& title, const std::string& detail)
{
    Slot*      s = this->find(id);
    const bool was_shown = s && !s->dismissed;
    if (!s) {
        this->slots.push_back({});
        s = &this->slots.back();
        s->item.id = id;
        s->item.order = this->next_order++;
    } else if (s->item.level == level && s->item.title == title && s->item.detail == detail) {
        return false;
    }
    // New words: told again, even when the old ones were put away.
    s->dismissed = level == Level::Info &&
                   std::ranges::contains(this->read, read_key(id, title, detail));
    s->item.level = level;
    s->item.title = title;
    s->item.detail = detail;
    return was_shown || !s->dismissed;
}

bool Store::clear(const std::string& id)
{
    for (auto it = this->slots.begin(); it != this->slots.end(); ++it) {
        if (it->item.id != id) continue;
        const bool shown = !it->dismissed;
        const std::string key = read_key(id, it->item.title, it->item.detail);
        std::erase(this->read, key);
        this->slots.erase(it);
        return shown;
    }
    return false;
}

bool Store::dismiss(const std::string& id)
{
    Slot* s = this->find(id);
    if (!s || s->dismissed || s->item.level != Level::Info) return false;
    s->dismissed = true;
    const std::string key = read_key(id, s->item.title, s->item.detail);
    if (!std::ranges::contains(this->read, key)) this->read.push_back(key);
    if (this->read.size() > MAX_READ) this->read.erase(this->read.begin());
    return true;
}

bool Store::has(const std::string& id) const
{
    const Slot* s = this->find(id);
    return s && !s->dismissed;
}

std::vector<Item> Store::visible() const
{
    std::vector<Item> out;
    for (Level level : { Level::Action, Level::Info })
        for (const Slot& s : this->slots)
            if (!s.dismissed && s.item.level == level) out.push_back(s.item);
    // slots keeps the order they were first set in: nothing to sort.
    return out;
}

int Store::action_count() const
{
    int n = 0;
    for (const Slot& s : this->slots) n += !s.dismissed && s.item.level == Level::Action;
    return n;
}

int Store::info_count() const
{
    int n = 0;
    for (const Slot& s : this->slots) n += !s.dismissed && s.item.level == Level::Info;
    return n;
}

std::vector<std::string> Store::read_keys() const
{
    return this->read;
}

void Store::set_read(const std::vector<std::string>& keys)
{
    this->read = keys;
    if (this->read.size() > MAX_READ) this->read.erase(this->read.begin(), this->read.end() - MAX_READ);
    for (Slot& s : this->slots)
        if (s.item.level == Level::Info &&
            std::find(this->read.begin(), this->read.end(), read_key(s.item.id, s.item.title, s.item.detail)) !=
                this->read.end())
            s.dismissed = true;
}

std::string read_key(const std::string& id, const std::string& title, const std::string& detail)
{
    char print[9];
    std::snprintf(print, sizeof(print), "%08x", hash_of(title + "\n" + detail));
    return id + ":" + print;
}

std::string keys_text(const std::vector<std::string>& keys)
{
    std::string out;
    for (const std::string& k : keys) out += k + "\n";
    return text_schema::with(KEYS_SCHEMA, out);
}

bool keys_newer(const std::string& text) { return text_schema::of(text) > KEYS_SCHEMA; }

std::vector<std::string> keys_of(const std::string& text)
{
    std::vector<std::string> out;
    if (keys_newer(text)) return out;
    size_t start = 0;
    while (start < text.size() && out.size() < MAX_READ) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (is_key(line)) out.push_back(line);
        start = end + 1;
    }
    return out;
}

}   // namespace notice_logic
