// core/sim_env.h: the PLAYSHELF_SIM_* variables, the environment's unless the
// web page changed one. A change is kept in a table of its own under a lock;
// the strings handed out are never freed, so a reader keeps a valid one
// whatever changes after it read. Each value is kept once, however often the
// page sets it (the time buttons set `skip` back and forth).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "core/sim_env.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <strings.h>

namespace
{

std::mutex s_lock;
// Allocated once, never freed: a static's destructor could run while a
// thread still reads them at exit.
// name -> the page's value (nullptr: removed).
std::map<std::string, const char*>& changes()
{
    static auto* m = new std::map<std::string, const char*>();
    return *m;
}
// Every value the page set: a std::set's elements never move.
std::set<std::string>& values()
{
    static auto* s = new std::set<std::string>();
    return *s;
}

}   // namespace

extern "C" const char* sim_env(const char* name)
{
    if (!name)
        return nullptr;
    {
        std::lock_guard<std::mutex> guard(s_lock);
        auto it = changes().find(name);
        if (it != changes().end())
            return it->second;
    }
    return std::getenv(name);
}

extern "C" bool sim_env_flag(const char* name)
{
    const char* v = sim_env(name);
    if (!v || !*v)
        return false;
    for (const char* off : {"0", "false", "off", "no"})
        if (strcasecmp(v, off) == 0)
            return false;
    return true;
}

extern "C" long long sim_env_int(const char* name, long long def, long long min, long long max)
{
    const char* v = sim_env(name);
    if (!v || !*v)
        return def;
    char* end = nullptr;
    errno = 0;
    const long long n = std::strtoll(v, &end, 10);
    if (end == v || *end)
        return def;
    // Out of range for a long long: the end it went past.
    if (n < min || (errno == ERANGE && n < 0))
        return min;
    if (n > max || errno == ERANGE)
        return max;
    return n;
}

extern "C" void sim_env_set(const char* name, const char* value)
{
    if (!name)
        return;
    std::lock_guard<std::mutex> guard(s_lock);
    changes()[name] = value && *value ? values().insert(value).first->c_str() : nullptr;
}
