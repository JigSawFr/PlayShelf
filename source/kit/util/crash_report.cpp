// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/crash_report.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_id.h"

#include "util/session_log.hpp"

namespace crash_report
{

namespace
{
char g_dir[512];
char g_build[160];
char g_system[256];
char g_session[96];
bool g_has_start = false;
std::chrono::steady_clock::time_point g_start;

// The file being written, and the crash dump's text: static, as nothing may
// be allocated.
char g_text[64 * 1024];
char g_details[16 * 1024];
char g_name[64];

void copy(char* to, size_t size, const std::string& from)
{
    std::snprintf(to, size, "%s", from.c_str());
}

// snprintf at the end of g_text.
size_t g_len = 0;
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
void add(const char* fmt, ...)
{
    if (g_len >= sizeof(g_text) - 1) return;
    va_list ap;
    va_start(ap, fmt);
    const int n = std::vsnprintf(g_text + g_len, sizeof(g_text) - g_len, fmt, ap);
    va_end(ap);
    if (n > 0) g_len = std::min(g_len + (size_t)n, sizeof(g_text) - 1);
}
}   // namespace

void prepare(const std::string& dir, const std::string& build, const std::string& system, const std::string& session)
{
    copy(g_dir, sizeof(g_dir), dir);
    copy(g_build, sizeof(g_build), build);
    copy(g_system, sizeof(g_system), system);
    copy(g_session, sizeof(g_session), session);
    if (!g_has_start) {
        g_start = std::chrono::steady_clock::now();
        g_has_start = true;
    }
}

const char* write(std::time_t now, const char* summary, const char* details, bool recent) noexcept
{
    g_name[0] = '\0';
    if (!g_dir[0]) return g_name;
    std::tm tmv{};
    char when[32] = "?", st[32] = "19700101_000000";
    if (localtime_r(&now, &tmv)) {
        std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", &tmv);
        std::strftime(st, sizeof(st), "%Y%m%d_%H%M%S", &tmv);
    }

    g_len = 0;
    g_text[0] = '\0';
    add("%s  %s: %s\n", when, g_build[0] ? g_build : APP_NAME, summary && *summary ? summary : "crash");
    if (g_system[0]) add("system  : %s\n", g_system);
    const long long after =
        g_has_start ? (long long)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - g_start).count()
                    : -1;
    if (g_session[0]) add("session : logs/sessions/%s, %lld s after the start\n", g_session, after);
    else if (after >= 0) add("session : no session file, %lld s after the start\n", after);
    if (details && *details) add("\n%s%s", details, details[std::strlen(details) - 1] == '\n' ? "" : "\n");
    if (recent) add("\nlast lines logged (oldest first):\n");
    if (recent && g_len < sizeof(g_text) - 1) {
        const size_t n = session_log::recent(g_text + g_len, sizeof(g_text) - g_len);
        g_len += n;
        if (n == 0) add("(none)\n");
    }

    // The first free name in the same second: _1, _2...
    // Created 0644 (as util/pt_log.cpp), not fopen's 0666.
    ::mkdir(g_dir, 0755);
    for (unsigned i = 0; i < 100; i++) {
        char name[sizeof(g_name)];
        if (i == 0) std::snprintf(name, sizeof(name), "crash_%s.txt", st);
        else std::snprintf(name, sizeof(name), "crash_%s_%u.txt", st, i);
        char path[sizeof(g_dir) + sizeof(name) + 2];
        std::snprintf(path, sizeof(path), "%s/%s", g_dir, name);
        const int fd = ::open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd < 0) {
            if (errno == EEXIST) continue;
            return g_name;
        }
        const bool ok = session_log::write_all(fd, g_text, g_len);
        ::close(fd);
        if (ok) std::snprintf(g_name, sizeof(g_name), "%s", name);
        return g_name;
    }
    return g_name;
}

namespace
{
void write_cpu(const CrashDump* dump) noexcept
{
    char summary[200] = "cpu exception";
    if (dump) {
        char what[160];
        crash_dump_summary(dump, what, sizeof(what));
        std::snprintf(summary, sizeof(summary), "cpu exception: %s", what);
        crash_dump_format(dump, g_details, sizeof(g_details));
    }
    const char* name = write(std::time(nullptr), summary, dump ? g_details : nullptr);
    session_log::end_crash(name);
}
}   // namespace

}   // namespace crash_report

extern "C" void crash_report_cpu(const CrashDump* dump)
{
    crash_report::write_cpu(dump);
}
