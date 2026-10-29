// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/update.hpp"

#include <aurore/extern/nlohmann/json.hpp>

#include "util/date.hpp"
#include "util/json_util.hpp"

namespace update
{

// `fourth`: a fourth number allowed ("1.6.3.0", sys-patch's), 0 when absent;
// without it, a fourth number is refused.
static bool parse_triplet(const std::string& text, int out[3], bool* suffixed, int* fourth = nullptr)
{
    size_t i = (!text.empty() && (text[0] == 'v' || text[0] == 'V')) ? 1 : 0;
    for (int part = 0; part < 3; part++) {
        if (part > 0) {
            if (i >= text.size() || text[i] != '.') return false;
            i++;
        }
        if (i >= text.size() || text[i] < '0' || text[i] > '9') return false;
        long v = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            v = v * 10 + (text[i] - '0');
            if (v > 0xFFFF) return false;
            i++;
        }
        out[part] = (int)v;
    }
    if (fourth) {
        *fourth = 0;
        if (i + 1 < text.size() && text[i] == '.' && text[i + 1] >= '0' && text[i + 1] <= '9') {
            long v = 0;
            for (i++; i < text.size() && text[i] >= '0' && text[i] <= '9'; i++) {
                v = v * 10 + (text[i] - '0');
                if (v > 0xFFFF) return false;
            }
            *fourth = (int)v;
        }
    }
    // Anything after the numbers must be a suffix ("-dev", "+abc").
    if (i < text.size() && text[i] != '-' && text[i] != '+') return false;
    if (suffixed) *suffixed = i < text.size();
    return true;
}

bool parse_version(const std::string& text, int out[3])
{
    return parse_triplet(text, out, nullptr);
}

int compare_versions(const std::string& a, const std::string& b)
{
    int va[4], vb[4];
    bool sa = false, sb = false;
    const bool oka = parse_triplet(a, va, &sa, &va[3]);
    const bool okb = parse_triplet(b, vb, &sb, &vb[3]);
    if (!oka || !okb) return (int)oka - (int)okb;
    for (int i = 0; i < 4; i++)
        if (va[i] != vb[i]) return va[i] < vb[i] ? -1 : 1;
    return (int)sb - (int)sa;   // "1.0.0-dev" < "1.0.0"
}

uint32_t parse_firmware(const std::string& text)
{
    int v[3];
    bool suffixed = false;
    if (!parse_triplet(text, v, &suffixed) || suffixed) return 0;
    if (v[0] > 255 || v[1] > 255 || v[2] > 255) return 0;
    return ((uint32_t)v[0] << 16) | ((uint32_t)v[1] << 8) | (uint32_t)v[2];
}

bool parse_compat(const std::string& json, Latest* out)
{
    return json_util::parse_with(json, [out](const nlohmann::json& j) {
        // A field missing or not a text reads as "", which neither parse takes.
        Latest l;
        l.version       = json_util::string_at(j, "version");
        l.fw_tested_max = parse_firmware(json_util::string_at(j, "fw_tested_max"));
        int v[3];
        if (!parse_version(l.version, v) || !l.fw_tested_max) return false;
        *out = l;
        return true;
    });
}

bool offer_at_start(const std::string& latest, const std::string& running, const Snooze& snooze,
                    const std::string& today)
{
    if (latest.empty() || compare_versions(latest, running) <= 0) return false;
    if (snooze.version.empty() || compare_versions(snooze.version, latest) != 0) return true;
    int64_t now, until;
    if (!date::day_number(today, &now) || !date::day_number(snooze.until, &until)) return true;
    const int64_t left = until - now;
    return left <= 0 || left > SNOOZE_MAX_DAYS;
}

Verdict decide(uint32_t console_fw, const std::string& app_version, const Latest& latest)
{
    if (compare_versions(latest.version, app_version) <= 0) return Verdict::UpToDate;
    return console_fw && console_fw <= latest.fw_tested_max ? Verdict::UpdateSupports : Verdict::UpdateNoSupport;
}

}   // namespace update
