// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/services.hpp"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <format>

namespace library
{

std::string normal_build_id(const std::string& build_id)
{
    if (build_id.empty() || build_id.size() > 64) return "";
    std::string out;
    for (char c : build_id) {
        if (!std::isxdigit((unsigned char)c)) return "";
        out += (char)std::toupper((unsigned char)c);
    }
    return out;
}

std::string cheat_path(const std::string& sd_root, uint64_t title_id, const std::string& build_id)
{
    const std::string bid = normal_build_id(build_id);
    if (bid.empty() || !title_id) return "";
    std::string root = sd_root;
    while (!root.empty() && root.back() == '/')
        root.pop_back();
    return root + "/atmosphere/contents/" + std::format("{:016X}", title_id) + "/cheats/" + bid + ".txt";
}

std::vector<std::string> installed_cheats(const std::string& sd_root, uint64_t title_id)
{
    std::vector<std::string> out;
    const std::string        any = cheat_path(sd_root, title_id, "0");
    if (any.empty()) return out;
    const std::string dir = any.substr(0, any.find_last_of('/'));
    DIR*              d = ::opendir(dir.c_str());
    if (!d) return out;
    while (const dirent* e = ::readdir(d)) {
        const std::string name = e->d_name;
        if (name.size() < 5 || name.compare(name.size() - 4, 4, ".txt") != 0) continue;
        const std::string bid = normal_build_id(name.substr(0, name.size() - 4));
        if (!bid.empty()) out.push_back(bid);
    }
    ::closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}

}   // namespace library
