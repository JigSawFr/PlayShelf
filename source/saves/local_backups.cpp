// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "saves/local_backups.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <map>

namespace local_backups
{

namespace
{
constexpr size_t STAMP = 19;   // "2026-11-14_11-12-54"
constexpr size_t TAG = 16;

bool digits(const std::string& s, size_t from, size_t n)
{
    if (from + n > s.size()) return false;
    for (size_t i = from; i < from + n; i++)
        if (!std::isdigit((unsigned char)s[i])) return false;
    return true;
}
}   // namespace

std::string dir_of(const std::string& data_dir, uint64_t title_id)
{
    return std::format("{}/saves/{:016X}", data_dir, title_id);
}

std::string user_tag(uint64_t uid0, uint64_t uid1)
{
    return std::format("{:016x}", uid0 ^ (uid1 * 0x9E3779B97F4A7C15ull));
}

std::string file_name(const std::string& date, const std::string& time, const std::string& tag)
{
    std::string t = time;
    std::replace(t.begin(), t.end(), ':', '-');
    return date + "_" + t + "_" + tag + ".zip";
}

bool is_ours(const std::string& name, std::string* tag)
{
    // "YYYY-MM-DD_HH-MM-SS_" + 16 lower-case hex digits + ".zip"
    if (name.size() != STAMP + 1 + TAG + 4) return false;
    if (!digits(name, 0, 4) || name[4] != '-' || !digits(name, 5, 2) || name[7] != '-' || !digits(name, 8, 2) || name[10] != '_' ||
        !digits(name, 11, 2) || name[13] != '-' || !digits(name, 14, 2) || name[16] != '-' || !digits(name, 17, 2) || name[19] != '_')
        return false;
    for (size_t i = STAMP + 1; i < STAMP + 1 + TAG; i++)
        if (!std::isxdigit((unsigned char)name[i]) || std::isupper((unsigned char)name[i])) return false;
    if (name.compare(STAMP + 1 + TAG, 4, ".zip") != 0) return false;
    if (tag) *tag = name.substr(STAMP + 1, TAG);
    return true;
}

std::vector<std::string> expired(const std::vector<std::string>& names, size_t keep)
{
    std::map<std::string, std::vector<std::string>> by_user;   // tag -> names
    for (const std::string& n : names) {
        std::string tag;
        if (is_ours(n, &tag)) by_user[tag].push_back(n);
    }
    std::vector<std::string> out;
    for (auto& [tag, list] : by_user) {
        std::sort(list.begin(), list.end());   // the date and time come first in the name: oldest first
        if (list.size() > keep) out.insert(out.end(), list.begin(), list.end() - (long)keep);
    }
    std::sort(out.begin(), out.end());
    return out;
}

}   // namespace local_backups
