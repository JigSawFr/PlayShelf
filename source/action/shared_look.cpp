// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/shared_look.hpp"

#include <string>

#include "bl_prefs.h"

namespace shared_look
{

namespace
{
struct Field
{
    BlPref       pref;
    std::string Look::* member;
};

constexpr Field FIELDS[] = {
    { BlPref_Language, &Look::language },
    { BlPref_Theme, &Look::theme },
    { BlPref_TextSize, &Look::text_size },
    { BlPref_Effects, &Look::effects },
};

std::string written(const BlPrefs& p)
{
    std::string out(2048, '\0');
    const size_t n = bl_prefs_write(&p, &out[0], out.size());
    out.resize(n);
    return out;
}
}   // namespace

std::string file(const std::string& sd_root)
{
    return sd_root + (sd_root.empty() || sd_root.back() != '/' ? "/" : "") + BL_PREFS_FILE;
}

std::string follow(const std::string& text, Look& look, const Valid& ok)
{
    BlPrefs p;
    bl_prefs_defaults(&p);
    bl_prefs_parse(&p, text.data(), text.size());
    bool seeded = false;
    for (const Field& f : FIELDS) {
        std::string& mine = look.*f.member;
        if (const char* shared = bl_prefs_get(&p, f.pref)) {
            if (ok(bl_prefs_key(f.pref), shared)) mine = shared;
        } else if (bl_prefs_set(&p, f.pref, mine.c_str()) && !mine.empty()) {
            seeded = true;   // no homebrew set it yet: PlayGuard's
        }
    }
    if (!seeded || !bl_prefs_may_write(&p)) return "";
    return written(p);
}

std::string publish(const std::string& text, const Look& look)
{
    BlPrefs p;
    bl_prefs_defaults(&p);
    bl_prefs_parse(&p, text.data(), text.size());
    if (!bl_prefs_may_write(&p)) return "";
    for (const Field& f : FIELDS) bl_prefs_set(&p, f.pref, (look.*f.member).c_str());
    const std::string out = written(p);
    return out == text ? "" : out;
}

}   // namespace shared_look
