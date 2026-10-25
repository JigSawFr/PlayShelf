// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/titles.hpp"

#include <algorithm>
#include <cstdio>
#include <map>

namespace library
{
namespace
{
// U+00C0..U+00FF without their accents, lower case ("" for × and ÷, kept).
const char* const LATIN1[64] = {
    "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "o", "",  "o", "u", "u", "u", "u", "y", "th", "ss",
    "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i",
    "d", "n", "o", "o", "o", "o", "o", "",  "o", "u", "u", "u", "u", "y", "th", "y",
};

std::string hex16(uint64_t id)
{
    char text[17];
    std::snprintf(text, sizeof text, "%016llx", (unsigned long long)id);
    return text;
}

bool name_less(const std::string& a, const std::string& b)
{
    return fold(a) < fold(b);
}
}   // namespace

std::string fold(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++) {
        const unsigned char c = (unsigned char)text[i];
        if (c < 0x80) {
            out += (char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
            continue;
        }
        if ((c == 0xc3 || c == 0xc5) && i + 1 < text.size()) {
            const unsigned char next = (unsigned char)text[i + 1];
            const uint32_t      cp = ((uint32_t)(c & 0x1f) << 6) | (next & 0x3f);
            const char*         plain = nullptr;
            if (cp >= 0xc0 && cp <= 0xff && LATIN1[cp - 0xc0][0]) plain = LATIN1[cp - 0xc0];
            if (cp == 0x152 || cp == 0x153) plain = "oe";
            if (plain) {
                out += plain;
                i++;
                continue;
            }
        }
        out += (char)c;
    }
    return out;
}

std::string clean_name(const std::string& name)
{
    std::string out;
    int         depth = 0;
    for (char c : name) {
        if (c == '[') depth++;
        else if (c == ']' && depth > 0) depth--;
        else if (depth == 0) out += c;
    }
    const size_t dot = out.find_last_of('.');
    if (dot != std::string::npos && out.size() - dot <= 5) {
        std::string ext = fold(out.substr(dot + 1));
        if (ext == "nsp" || ext == "nsz" || ext == "xci" || ext == "xcz") out.resize(dot);
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '_' || out.back() == '-'))
        out.pop_back();
    while (!out.empty() && out.front() == ' ')
        out.erase(out.begin());
    return out.empty() ? name : out;
}

long latest_update(const Title& t)
{
    return t.updates.empty() ? -1 : (long)t.updates.front();
}

std::vector<Title> group_titles(const Catalog& catalog)
{
    std::vector<Title>          titles;
    std::map<uint64_t, size_t>  by_id;
    const auto& items = catalog.items;
    for (size_t i = 0; i < items.size(); i++) {
        const Item& it = items[i];
        Title*      t = nullptr;
        if (it.title_id) {
            const uint64_t base = base_of(it.title_id);
            const auto     found = by_id.find(base);
            if (found == by_id.end()) {
                by_id[base] = titles.size();
                titles.emplace_back();
                titles.back().id = base;
            }
            t = &titles[by_id[base]];
        } else {
            titles.emplace_back();
            t = &titles.back();
        }
        const Kind kind = it.kind != Kind::Unknown ? it.kind : it.title_id ? kind_of(it.title_id) : Kind::Base;
        if (kind == Kind::Update) t->updates.push_back(i);
        else if (kind == Kind::Dlc) t->dlcs.push_back(i);
        // A second base game of the same id (another dump): the newest.
        else if (t->base < 0 || items[(size_t)t->base].version < it.version) t->base = (long)i;
        else t->dlcs.push_back(i);
        t->size += it.size;
        if (std::find(t->sections.begin(), t->sections.end(), it.section) == t->sections.end())
            t->sections.push_back(it.section);
    }

    for (Title& t : titles) {
        std::sort(t.updates.begin(), t.updates.end(),
                  [&](size_t a, size_t b) { return items[a].version > items[b].version; });
        std::sort(t.dlcs.begin(), t.dlcs.end(), [&](size_t a, size_t b) { return name_less(items[a].name, items[b].name); });
        // Its name: the titledb's, else the base game's, else its first
        // item's, without the file's tags.
        const auto known = t.id ? catalog.titles.find(t.id) : catalog.titles.end();
        const Item& first = items[t.base >= 0 ? (size_t)t.base : !t.updates.empty() ? t.updates.front() : t.dlcs.front()];
        if (known != catalog.titles.end() && !known->second.name.empty()) t.name = known->second.name;
        else t.name = clean_name(first.name);
        // The base game's icon and date first, else any item's.
        std::vector<size_t> order;
        if (t.base >= 0) order.push_back((size_t)t.base);
        order.insert(order.end(), t.updates.begin(), t.updates.end());
        order.insert(order.end(), t.dlcs.begin(), t.dlcs.end());
        for (size_t i : order) {
            if (t.icon_url.empty()) t.icon_url = items[i].icon_url;
            if (!t.release_date) t.release_date = items[i].release_date;
        }
    }
    return titles;
}

std::vector<size_t> select_titles(const std::vector<Title>& titles, const std::string& query, Sort sort,
                                  const std::string& section)
{
    const std::string   q = fold(query);
    // A title id is looked for by 8 hex digits at least: fewer would match
    // most games ("0100…").
    const bool by_id = q.size() >= 8 && q.find_first_not_of("0123456789abcdef") == std::string::npos;
    std::vector<size_t> out;
    std::vector<std::string> names(titles.size());
    for (size_t i = 0; i < titles.size(); i++) {
        const Title& t = titles[i];
        if (!section.empty() && std::find(t.sections.begin(), t.sections.end(), section) == t.sections.end()) continue;
        names[i] = fold(t.name);
        if (!q.empty() && names[i].find(q) == std::string::npos && !(by_id && t.id && hex16(t.id).find(q) != std::string::npos))
            continue;
        out.push_back(i);
    }
    std::stable_sort(out.begin(), out.end(), [&](size_t a, size_t b) {
        const Title& x = titles[a];
        const Title& y = titles[b];
        if (sort == Sort::Newest && x.release_date != y.release_date) return x.release_date > y.release_date;
        if (sort == Sort::Largest && x.size != y.size) return x.size > y.size;
        return names[a] < names[b];
    });
    return out;
}

}   // namespace library
