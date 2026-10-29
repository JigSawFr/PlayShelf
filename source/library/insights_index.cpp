// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/insights_index.hpp"

#include <aurore/extern/nlohmann/json.hpp>

#include "library/signals.hpp"
#include "util/sha256.hpp"

namespace library
{

namespace
{
// A file's path in the data branch: lower-case letters, digits, '_', '-',
// '.' and '/' (prices/FR.json), upper case for a country; no "..".
bool is_name(const std::string& name)
{
    if (name.empty() || name.size() > InsightsIndex::MAX_NAME || name.front() == '/' || name.contains(".."))
        return false;
    for (const char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/'))
            return false;
    return true;
}

bool is_hash(const std::string& hash)
{
    if (hash.size() != 16) return false;
    for (const char c : hash)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
}   // namespace

bool InsightsIndex::from_json(const std::string& text)
{
    this->files.clear();
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", 0) != 1 || !root.contains("files") || !root["files"].is_object()) return false;
    for (const auto& [name, value] : root["files"].items()) {
        if (this->files.size() >= MAX_FILES) break;
        if (!is_name(name) || !value.is_object()) continue;
        const auto generated = value.find("generated"), bytes = value.find("bytes"), sha = value.find("sha256");
        if (generated == value.end() || !generated->is_string() || !is_day(generated->get<std::string>())) continue;
        if (bytes == value.end() || !bytes->is_number_unsigned() || bytes->get<uint64_t>() > MAX_BYTES) continue;
        if (sha == value.end() || !sha->is_string() || !is_hash(sha->get<std::string>())) continue;
        this->files[name] = { generated->get<std::string>(), bytes->get<uint64_t>(), sha->get<std::string>() };
    }
    return true;
}

const IndexEntry* InsightsIndex::find(const std::string& name) const
{
    const auto it = this->files.find(name);
    return it == this->files.end() ? nullptr : &it->second;
}

std::string short_hash(const std::string& bytes)
{
    Sha256 sha;
    sha.update(bytes.data(), bytes.size());
    return sha.hex().substr(0, 16);
}

bool must_fetch(const InsightsIndex* index, const std::string& name, const std::string& cached)
{
    const IndexEntry* entry = index ? index->find(name) : nullptr;
    if (!entry || cached.empty()) return true;
    return cached.size() != entry->bytes || short_hash(cached) != entry->sha256;
}

}   // namespace library
