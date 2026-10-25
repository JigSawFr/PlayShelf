// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/diagnostics_logic.hpp"

#include <fmt/format.h>

namespace diagnostic
{

const char* storage_name(SysStorage storage)
{
    switch (storage) {
        case SysStorage_EmuMMC: return "emuMMC";
        case SysStorage_SysMMC: return "sysMMC";
        default:                return nullptr;
    }
}

std::string bytes(uint64_t n)
{
    if (n >= (1ULL << 30)) return fmt::format("{:.1f} GiB", (double)n / (1ULL << 30));
    if (n >= (1ULL << 20)) return fmt::format("{:.1f} MiB", (double)n / (1ULL << 20));
    return fmt::format("{} KiB", (n + 1023) / 1024);
}

std::string first_line(const std::string& text)
{
    return text.substr(0, text.find('\n'));
}

std::string patch_line(const patches::Report& pr)
{
    std::string files, gaps;
    for (const auto& f : pr.files) files += (files.empty() ? "" : ",") + f;
    for (const auto& g : pr.gaps) gaps += (gaps.empty() ? " (not patched: " : ", ") + g;
    if (!gaps.empty()) gaps += ")";
    static const char* const STATUS[] = { "none", "sys-patch", "sys-patch incomplete", "sigpatch files only" };
    static const char* const ISSUE[]  = { "none", "not at boot", "no log", "stale log", "skipped", "not patched" };
    return fmt::format(
        "{} (issue: {}; sys-patch installed={} boot2={} log={}{}; fs={} ldr={} es={}{}; files={})",
        STATUS[(int)patches::status(pr)], ISSUE[(int)patches::issue(pr)],
        pr.syspatch_installed ? 1 : 0, pr.syspatch_at_boot ? 1 : 0,
        !pr.log_present ? "none" : (pr.log_current ? "current" : "stale"),
        pr.log_present ? fmt::format(" fw={} emummc={} version={}", pr.log_fw, pr.log_emummc ? 1 : 0,
                                     pr.log_version.empty() ? std::string("?") : pr.log_version)
                       : std::string(),
        patches::module_name(pr.fs), patches::module_name(pr.ldr), patches::module_name(pr.es),
        gaps,
        files.empty() ? std::string("none") : files);
}

}   // namespace diagnostic
