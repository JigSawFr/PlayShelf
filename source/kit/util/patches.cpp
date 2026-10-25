// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/patches.hpp"

#include <cstdio>
#include <sys/stat.h>

#include <borealis/cadran/util/text.hpp>

#include "util/paths.hpp"

namespace patches
{

namespace
{
bool has_ips(const std::string& dir)
{
    return !paths::list_files(dir, ".ips").empty() || !paths::list_files(dir, ".IPS").empty();
}

// "23.0.1" as one number; false when it is not three dotted numbers.
bool parse_version(const std::string& text, unsigned long* out)
{
    unsigned a = 0, b = 0, c = 0;
    char end = 0;
    if (std::sscanf(text.c_str(), "%u.%u.%u%c", &a, &b, &c, &end) != 3) return false;
    *out = (unsigned long)a << 16 | (b & 0xFF) << 8 | (c & 0xFF);
    return true;
}

std::string join(const std::string& root, const char* rel)
{
    if (root.empty() || root.back() == '/') return root + rel;
    return root + "/" + rel;
}
}   // namespace

Ini parse_ini(const std::string& text)
{
    Ini ini;
    std::string section;
    for (const std::string& raw : text::lines(text)) {
        const std::string line = text::trim_space(raw);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = text::trim_space(line.substr(1, line.size() - 2));
            ini[section];
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        ini[section][text::trim_space(line.substr(0, eq))] = text::trim_space(line.substr(eq + 1));
    }
    return ini;
}

bool pattern_for(const std::string& name, const std::string& fw)
{
    const size_t us = name.rfind('_');
    unsigned long v = 0, lo = 0, hi = 0;
    if (us == std::string::npos || !parse_version(fw, &v)) return false;
    std::string range = name.substr(us + 1);
    if (!range.empty() && range.back() == '+') {
        if (!parse_version(range.substr(0, range.size() - 1), &lo)) return false;
        hi = ~0ul;
    } else {
        const size_t dash = range.find('-');
        if (dash == std::string::npos || !parse_version(range.substr(0, dash), &lo) ||
            !parse_version(range.substr(dash + 1), &hi))
            return false;
    }
    return lo <= v && v <= hi;
}

Module module_state(const Ini& ini, const std::string& name, const std::string& fw, std::vector<std::string>* gaps)
{
    auto it = ini.find(name);
    if (it == ini.end() || it->second.empty()) return Module::Missing;
    bool syspatch = false, file = false, failed = false;
    bool all_skipped = true, all_disabled = true;
    for (const auto& kv : it->second) {
        const std::string& v = kv.second;
        syspatch |= text::starts_with(v, "Patched (sys-patch)");
        file     |= text::starts_with(v, "Patched (file)");
        failed   |= text::starts_with(v, "Failed");
        all_skipped  &= text::starts_with(v, "Skipped");
        all_disabled &= text::starts_with(v, "Disabled");
    }
    if (syspatch || file) {
        bool partial = false;
        for (const auto& kv : it->second) {
            const std::string& v = kv.second;
            // Skipped: out of its Atmosphère range, or patching turned off.
            if (text::starts_with(v, "Patched") || text::starts_with(v, "Skipped") || fw.empty() ||
                !pattern_for(kv.first, fw))
                continue;
            partial = true;
            if (gaps) gaps->push_back(name + ":" + kv.first + "=" + v);
        }
        if (partial) return Module::Partial;
        return syspatch ? Module::SysPatch : Module::File;
    }
    if (failed) return Module::Failed;
    if (all_skipped) return Module::Skipped;
    if (all_disabled) return Module::Disabled;
    return Module::Unpatched;
}

Report detect(const std::string& root, const std::string& fw, bool emummc)
{
    Report r;
    r.syspatch_installed = paths::file_exists(join(root, "atmosphere/contents/420000000000000B/exefs.nsp"));
    r.syspatch_at_boot   = paths::file_exists(join(root, "atmosphere/contents/420000000000000B/flags/boot2.flag"));

    std::string text;
    if (paths::read_file(join(root, "config/sys-patch/log.ini"), text) && !text.empty()) {
        Ini ini = parse_ini(text);
        r.log_present = true;
        auto stats = ini.find("stats");
        if (stats != ini.end()) {
            auto f = stats->second.find("fw_version");
            auto e = stats->second.find("is_emummc");
            if (f != stats->second.end()) r.log_fw = f->second;
            if (e != stats->second.end()) r.log_emummc = e->second == "1";
            auto v = stats->second.find("version");
            if (v != stats->second.end()) r.log_version = v->second;
            r.log_current = f != stats->second.end() && e != stats->second.end() &&
                            r.log_fw == fw && r.log_emummc == emummc;
        }
        r.fs  = module_state(ini, "fs", fw, &r.gaps);
        r.ldr = module_state(ini, "ldr", fw, &r.gaps);
        r.es  = module_state(ini, "es", fw, &r.gaps);
    }

    if (has_ips(join(root, "atmosphere/exefs_patches/es_patches")))     r.files.push_back("es");
    if (has_ips(join(root, "atmosphere/kip_patches/fs_patches")))       r.files.push_back("fs");
    if (has_ips(join(root, "atmosphere/kip_patches/loader_patches")))   r.files.push_back("loader");
    if (has_ips(join(root, "atmosphere/exefs_patches/nfim_ctest")))     r.files.push_back("nifm");
    if (paths::file_exists(join(root, "bootloader/patches.ini")))                   r.files.push_back("hekate");
    return r;
}

bool emummc_configured(const std::string& root)
{
    std::string text;
    if (!paths::read_file(join(root, "emummc/emummc.ini"), text)) return false;
    const Ini ini = parse_ini(text);
    auto section = ini.find("emummc");
    if (section == ini.end()) return false;
    auto enabled = section->second.find("enabled");
    return enabled != section->second.end() && !enabled->second.empty() && enabled->second != "0";
}

bool patched(Module m)
{
    return m == Module::SysPatch || m == Module::File;
}

std::vector<std::string> unpatched(const Report& r)
{
    std::vector<std::string> out;
    if (!patched(r.fs)) out.push_back("fs");
    if (!patched(r.ldr)) out.push_back("ldr");
    if (!patched(r.es)) out.push_back("es");
    return out;
}

Issue issue(const Report& r)
{
    if (!r.syspatch_installed) return Issue::None;
    if (!r.syspatch_at_boot) return Issue::NotAtBoot;
    if (!r.log_present) return Issue::NoLog;
    if (!r.log_current) return Issue::StaleLog;
    if (r.fs == Module::Skipped && r.ldr == Module::Skipped && r.es == Module::Skipped) return Issue::Skipped;
    if (!unpatched(r).empty()) return Issue::NotPatched;
    return Issue::None;
}

Status status(const Report& r)
{
    if (!r.syspatch_installed) return r.files.empty() ? Status::None : Status::FilesOnly;
    return issue(r) == Issue::None ? Status::SysPatch : Status::SysPatchIncomplete;
}

const char* module_name(Module m)
{
    switch (m) {
        case Module::Missing:   return "missing";
        case Module::SysPatch:  return "sys-patch";
        case Module::File:      return "file";
        case Module::Failed:    return "failed";
        case Module::Skipped:   return "skipped";
        case Module::Disabled:  return "disabled";
        case Module::Unpatched: return "unpatched";
        case Module::Partial:   return "partial";
    }
    return "?";
}

bool mixed(const Report& r)
{
    if (!r.syspatch_installed) return false;
    for (const std::string& f : r.files)
        if (f == "es" || f == "fs" || f == "loader") return true;
    return false;
}

bool can_set_aside(const Report& r)
{
    return mixed(r) && r.syspatch_at_boot && r.log_present && r.log_current;
}

bool set_aside(const std::string& root, const std::string& dest, std::vector<std::string>* moved, std::string* error)
{
    for (const char* dir : SIGPATCH_DIRS) {
        const std::string from = join(root, dir);
        if (!has_ips(from)) continue;
        const std::string to = join(dest, dir);
        const size_t slash = to.rfind('/');
        struct stat st;
        const bool taken = stat(to.c_str(), &st) == 0;   // never over an earlier set aside
        if (taken || !paths::ensure_dir(to.substr(0, slash)) || std::rename(from.c_str(), to.c_str()) != 0) {
            if (error) *error = std::string("cannot move ") + dir;
            return false;
        }
        if (moved) moved->push_back(dir);
    }
    return true;
}

}   // namespace patches
