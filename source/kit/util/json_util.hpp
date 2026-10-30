// json_util — reading the JSON files and answers the app is given without
// an exception: each field read as its type or not at all, a text that is no
// JSON as a false. Header-only, plain C++ (nlohmann/json, from Aurore).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <exception>
#include <string>

#include <aurore/extern/nlohmann/json.hpp>

namespace json_util
{

// `text` parsed into `out`; false (and `out` discarded) when it is not JSON.
// Never throws.
inline bool parse(const std::string& text, nlohmann::json* out)
{
    *out = nlohmann::json::parse(text, nullptr, false);
    return !out->is_discarded();
}

// `text` parsed and handed to `read`, whose answer it returns; false when the
// text is not JSON or when `read` throws (a field of another type taken with
// json::value(), memory running out). For a reader that walks a whole
// document rather than checking each field's type first.
template <typename F>
bool parse_with(const std::string& text, F read)
{
    try {
        return read(nlohmann::json::parse(text));
    } catch (const std::exception&) {
        return false;
    }
}

// j[key] when `j` is an object holding a string there, else "".
inline std::string string_at(const nlohmann::json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

// j[key] when it is the boolean true; false otherwise.
inline bool flag_at(const nlohmann::json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_boolean() && it->get<bool>();
}

// &j[key] when `j` is an object holding an array there, else nullptr.
inline const nlohmann::json* array_at(const nlohmann::json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_array() ? &*it : nullptr;
}

// &j[key] when `j` is an object holding an object there, else nullptr.
inline const nlohmann::json* object_at(const nlohmann::json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_object() ? &*it : nullptr;
}

// j[key] into `out` when it is an integer; false (and `out` untouched) otherwise.
inline bool integer_at(const nlohmann::json& j, const char* key, int64_t* out)
{
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number_integer()) return false;
    *out = it->get<int64_t>();
    return true;
}

// j[key] when it is an integer, else `fallback`.
inline int64_t integer_or(const nlohmann::json& j, const char* key, int64_t fallback = 0)
{
    int64_t v = fallback;
    integer_at(j, key, &v);
    return v;
}

// The document's "schema" is newer than `known` (a missing one counts as 1):
// a later version wrote it, so a reader takes it as absent and never writes
// over it (docs/contributing/formats.md).
inline bool newer_schema(const nlohmann::json& j, int64_t known)
{
    return j.is_object() && integer_or(j, "schema", 1) > known;
}

}   // namespace json_util
