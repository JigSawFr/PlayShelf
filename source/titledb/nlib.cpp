// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "titledb/nlib.hpp"

#include <algorithm>
#include <string>

#include <aurore/extern/nlohmann/json.hpp>

#include "titledb/pack.hpp"
#include "util/number.hpp"
#include "util/title_id.hpp"

namespace titledb
{
namespace
{
// A base game ends in 000 with bit 12 clear (library::kind_of).
bool is_base(uint64_t id)
{
    return id && !(id & 0x1fff);
}

std::string game_url(uint64_t base_id, const std::string& rest)
{
    return "https://api.nlib.cc/nx/" + title_id::text(base_id) + rest;
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

// Its strings (`max` at most, each `size` bytes at most), the empty ones
// and the ones seen already left out.
std::vector<std::string> list_of(const nlohmann::json& j, const char* key, size_t max, size_t size)
{
    std::vector<std::string> out;
    const auto               it = j.find(key);
    if (it == j.end() || !it->is_array()) return out;
    for (const auto& v : *it) {
        if (out.size() >= max) break;
        const std::string s = v.is_string() ? safe_text(v.get<std::string>(), size) : "";
        if (!s.empty() && !std::ranges::contains(out, s)) out.push_back(s);
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
    return game_url(base_id, "/icon/" + std::to_string(size));
}

std::string nlib_language(const std::string& locale)
{
    const std::string base = locale.substr(0, locale.find('-'));
    for (const char* k : NLIB_LANGUAGES)
        if (base == k) return k;
    return "en";
}

std::string nlib_details_url(uint64_t base_id, const std::string& language)
{
    if (!is_base(base_id)) return "";
    return game_url(base_id, "?lang=" + nlib_language(language) +
                                 "&fields=name,publisher,developer,releaseDate,category,intro,description,screens,"
                                 "numberOfPlayers,languages,ratingContent,isDemo,nsuId");
}

std::string nlib_banner_url(uint64_t base_id)
{
    return is_base(base_id) ? game_url(base_id, "/banner/480p") : "";
}

std::string nlib_screen_url(uint64_t base_id, int n)
{
    if (!is_base(base_id) || n < 1 || n > NLIB_MAX_SCREENS) return "";
    return game_url(base_id, "/screen/" + std::to_string(n));
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
    if (id->get_ref<const std::string&>() != title_id::text(base_id)) return false;

    Details d;
    d.name = text_of(j, "name", 256);
    d.publisher = text_of(j, "publisher", 128);
    d.release_date = text_of(j, "releaseDate", 10);
    if (!is_date(d.release_date)) d.release_date.clear();
    d.developer = text_of(j, "developer", 128);
    d.categories = list_of(j, "category", 8, 64);
    d.languages = list_of(j, "languages", 16, 8);
    d.rating = list_of(j, "ratingContent", 16, 64);
    const auto players = j.find("numberOfPlayers");
    if (players != j.end() && players->is_number_integer()) {
        const int64_t n = players->get<int64_t>();
        d.players = n < 0 ? 0 : n > 99 ? 99 : (int)n;
    }
    const auto demo = j.find("isDemo");
    d.demo = demo != j.end() && demo->is_boolean() && demo->get<bool>();
    // Its eShop id: digits, as a string or a number.
    const auto nsu = j.find("nsuId");
    if (nsu != j.end() && nsu->is_number_unsigned()) d.nsu_id = nsu->get<uint64_t>();
    else if (nsu != j.end() && nsu->is_string()) {
        const std::string& s = nsu->get_ref<const std::string&>();
        if (const auto n = number::parse<uint64_t>(s)) d.nsu_id = *n;
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
