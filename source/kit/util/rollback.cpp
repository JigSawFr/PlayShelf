// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/rollback.hpp"

#include <cstdio>

#include <aurore/extern/nlohmann/json.hpp>

#include "util/json_util.hpp"
#include "util/paths.hpp"

namespace rollback
{

namespace
{
using nlohmann::json;

bool read_json(const std::string& file, json* out)
{
    std::string text;
    if (!paths::read_file(file, text)) return false;
    return json_util::parse_with(text, [out](json j) {
        *out = std::move(j);
        return out->is_object();
    });
}
}   // namespace

std::string previous_file(const std::string& nro)
{
    return nro + ".previous";
}

bool read_label(const std::string& json_file, std::string* label)
{
    json j;
    if (!read_json(json_file, &j)) return false;
    const std::string l = json_util::string_at(j, "label");
    if (l.empty()) return false;
    *label = l;
    return true;
}

bool write_label(const std::string& json_file, const std::string& label)
{
    std::string err;
    return paths::atomic_write(json_file, json{ { "schema", SCHEMA }, { "label", label } }.dump() + "\n", &err);
}

bool read_check(const std::string& json_file, Check* out)
{
    json j;
    if (!read_json(json_file, &j)) return false;
    Check c;
    c.label = json_util::string_at(j, "label");
    if (j.contains("starts") && j["starts"].is_number_integer()) c.starts = j["starts"].get<int>();
    if (c.starts < 0) c.starts = 0;
    *out = c;
    return true;
}

bool write_check(const std::string& json_file, const Check& check)
{
    std::string err;
    return paths::atomic_write(json_file, json{ { "schema", SCHEMA }, { "label", check.label }, { "starts", check.starts } }.dump() + "\n", &err);
}

bool offer_at_start(bool checked, int starts_before, bool have_previous)
{
    return have_previous && checked && starts_before >= 1;
}

bool go_back_allowed(bool pin_entered, bool no_pin_set)
{
    return pin_entered || no_pin_set;
}

void recover(const std::string& nro)
{
    paths::recover_replace(nro, previous_file(nro));
}

bool swap(const std::string& nro, std::string* error)
{
    const std::string prev = previous_file(nro);
    recover(nro);   // a swap or an install stopped half-way: finished first
    if (!paths::file_exists(prev)) {
        if (error) *error = "no previous build";
        return false;
    }
    // The build that ran becomes the previous one (none when there was none).
    return paths::replace_file(nro, prev, prev, error);
}

}   // namespace rollback
