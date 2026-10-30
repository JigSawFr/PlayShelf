// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/data_cleanup.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <utility>

namespace data_cleanup
{

namespace
{
std::vector<std::string>& kept_logs()
{
    static std::vector<std::string> kept;
    return kept;
}

// The files under `dir` and its subfolders, with their sizes. `keep(rel)`
// leaves out a file by its path from the folder collect() started at
// ("uploads/upload_….url", "play_timer/block.json").
template <typename Keep>
void collect(const std::string& dir, Plan& plan, Keep keep, const std::string& rel = "")
{
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    std::vector<std::string> subdirs;
    while (dirent* e = readdir(d)) {
        const std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        const std::string path = dir + "/" + name;
        const std::string sub = rel.empty() ? name : rel + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            subdirs.push_back(name);
        } else if (S_ISREG(st.st_mode) && !keep(sub)) {
            plan.files.push_back(path);
            plan.bytes += (uint64_t)st.st_size;
        }
    }
    closedir(d);
    for (const std::string& name : subdirs) collect(dir + "/" + name, plan, keep, rel.empty() ? name : rel + "/" + name);
}

Plan sorted(Plan plan)
{
    std::sort(plan.files.begin(), plan.files.end());
    return plan;
}
}   // namespace

Plan cache(const std::string& data_dir)
{
    Plan plan;
    collect(data_dir + "/cache", plan, [](const std::string&) { return false; });
    // Each system's play-data cache (util/nand.hpp).
    if (DIR* d = opendir((data_dir + "/nand").c_str())) {
        std::vector<std::string> systems;
        while (dirent* e = readdir(d)) {
            const std::string n = e->d_name;
            if (n != "." && n != "..") systems.push_back(n);
        }
        closedir(d);
        for (const std::string& n : systems)
            collect(data_dir + "/nand/" + n + "/cache", plan, [](const std::string&) { return false; });
    }
    return sorted(plan);
}

Plan logs(const std::string& data_dir, const std::string& session)
{
    Plan plan;
    const std::string current = session.empty() ? std::string() : "sessions/" + session;
    collect(data_dir + "/logs", plan, [&current](const std::string& rel) {
        return rel.starts_with("uploads/") || std::ranges::contains(kept_logs(), rel) ||
               (!current.empty() && rel == current);
    });
    return sorted(plan);
}

void keep_log(std::string rel)
{
    if (!std::ranges::contains(kept_logs(), rel)) kept_logs().push_back(std::move(rel));
}

Outcome remove(const Plan& plan)
{
    Outcome out;
    for (const std::string& path : plan.files) {
        struct stat st;
        if (stat(path.c_str(), &st) != 0) continue;   // gone since the plan
        if (std::remove(path.c_str()) == 0) {
            out.removed++;
            out.bytes += (uint64_t)st.st_size;
        } else if (errno != ENOENT) {
            out.failed++;
        }
    }
    return out;
}

Unit size_unit(uint64_t bytes, uint64_t* value)
{
    constexpr uint64_t KB = 1024, MB = 1024 * 1024;
    if (bytes < MB) {
        *value = (bytes + KB - 1) / KB;   // rounded up: a small file is not "0 KB"
        return Unit::KB;
    }
    *value = (bytes + MB / 2) / MB;
    return Unit::MB;
}

}   // namespace data_cleanup
