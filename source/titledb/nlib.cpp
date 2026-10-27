// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "titledb/nlib.hpp"

#include <cinttypes>
#include <cstdio>
#include <string>

#include <borealis/extern/nlohmann/json.hpp>

#include "titledb/pack.hpp"

namespace titledb
{
namespace
{
// A base game ends in 000 with bit 12 clear (library::kind_of).
bool is_base(uint64_t id)
{
    return id && !(id & 0x1fff);
}

std::string game_url(uint64_t base_id, const char* rest)
{
    char url[128];
    std::snprintf(url, sizeof url, "https://api.nlib.cc/nx/%016" PRIX64 "%s", base_id, rest);
    return url;
}

std::string text_of(const nlohmann::json& j, const char* key, size_t max)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_string() ? safe_text(it->get<std::string>(), max) : "";
}

// As text_of(), each line made safe on its own: the line breaks kept, runs
// of more than one blank line made one, none at the ends.
std::string lines_of(const nlohmann::json& j, const char* key, size_t max)
{
    const auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return "";
    const std::string& text = it->get_ref<const std::string&>();
    std::string        out;
    size_t             blank = 0;
    for (size_t from = 0; from <= text.size();) {
        size_t to = text.find('\n', from);
        if (to == std::string::npos) to = text.size();
        std::string line = safe_text(text.substr(from, to - from), max);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (line.empty()) blank++;
        else {
            const char* sep = out.empty() ? "" : blank ? "\n\n" : "\n";
            if (out.size() + std::char_traits<char>::length(sep) + line.size() > max) break;
            out += sep;
            out += line;
            blank = 0;
        }
        from = to + 1;
    }
    return out;
}

bool is_date(const std::string& s)
{
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (size_t i : { 0, 1, 2, 3, 5, 6, 8, 9 })
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}
}   // namespace

std::string nlib_icon_url(uint64_t base_id, int size)
{
    if (!is_base(base_id) || size < 30 || size > 4096) return "";
    return game_url(base_id, ("/icon/" + std::to_string(size)).c_str());
}

std::string nlib_language(const std::string& locale)
{
    static const char* const KNOWN[] = { "en", "fr", "ja", "es", "de", "nl", "pt", "it", "zh", "ko", "ru" };
    const std::string base = locale.substr(0, locale.find('-'));
    for (const char* k : KNOWN)
        if (base == k) return k;
    return "en";
}

std::string nlib_details_url(uint64_t base_id, const std::string& language)
{
    if (!is_base(base_id)) return "";
    return game_url(base_id, ("?lang=" + nlib_language(language) +
                              "&fields=name,publisher,releaseDate,category,intro,description,screens").c_str());
}

std::string nlib_banner_url(uint64_t base_id)
{
    return is_base(base_id) ? game_url(base_id, "/banner/480p") : "";
}

std::string nlib_screen_url(uint64_t base_id, int n)
{
    if (!is_base(base_id) || n < 1 || n > NLIB_MAX_SCREENS) return "";
    return game_url(base_id, ("/screen/" + std::to_string(n)).c_str());
}

bool parse_nlib_details(const std::string& json, uint64_t base_id, Details* out)
{
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (!j.is_object()) return false;
    const auto ok = j.find("success");
    if (ok != j.end() && ok->is_boolean() && !ok->get<bool>()) return false;
    // Its id, the one asked (the API writes it in capitals).
    const auto id = j.find("id");
    if (id == j.end() || !id->is_string()) return false;
    char want[17];
    std::snprintf(want, sizeof want, "%016" PRIX64, base_id);
    if (id->get_ref<const std::string&>() != want) return false;

    Details d;
    d.name = text_of(j, "name", 256);
    d.publisher = text_of(j, "publisher", 128);
    d.release_date = text_of(j, "releaseDate", 10);
    if (!is_date(d.release_date)) d.release_date.clear();
    const auto cat = j.find("category");
    if (cat != j.end() && cat->is_array())
        for (const auto& c : *cat) {
            if (d.categories.size() >= 8) break;
            const std::string s = c.is_string() ? safe_text(c.get<std::string>(), 64) : "";
            if (!s.empty()) d.categories.push_back(s);
        }
    d.intro = lines_of(j, "intro", 1024);
    d.description = lines_of(j, "description", 8192);
    const auto screens = j.find("screens");
    if (screens != j.end() && screens->is_object()) {
        const auto count = screens->find("count");
        if (count != screens->end() && count->is_number_integer()) {
            const int64_t n = count->get<int64_t>();
            d.screens = n < 0 ? 0 : n > NLIB_MAX_SCREENS ? NLIB_MAX_SCREENS : (int)n;
        }
    }
    *out = std::move(d);
    return true;
}

}   // namespace titledb
