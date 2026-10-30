// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/config.hpp"

#include <aurore/extern/nlohmann/json.hpp>
#include <algorithm>
#include <initializer_list>

#include "library/prices.hpp"
#include "util/date.hpp"
#include "util/json_util.hpp"
#include "util/paths.hpp"

namespace config
{

static Config s_config;
static Loaded s_loaded = Loaded::Missing;
// The file as last read: its keys this build does not know are written back.
static nlohmann::json s_file = nlohmann::json::object();

Config& get() { return s_config; }
Loaded loaded() { return s_loaded; }

namespace
{
// One field at a time: a field of the wrong type keeps its default, the
// others are still read (a hand-edited or older file loses one setting, not
// all of them). Each reader returns false when the field is
// there but refused, so related fields can be dropped together.
bool read_string(const nlohmann::json& j, const char* key, std::string& out)
{
    auto it = j.find(key);
    if (it == j.end()) return true;
    if (!it->is_string()) return false;
    out = it->get<std::string>();
    return true;
}

// An array's strings (its other values left out); `out` kept when the key
// is missing or not an array.
bool read_strings(const nlohmann::json& j, const char* key, std::vector<std::string>& out)
{
    const nlohmann::json* a = json_util::array_at(j, key);
    if (!a) return j.find(key) == j.end();
    out.clear();
    for (const nlohmann::json& v : *a)
        if (v.is_string()) out.push_back(v.get<std::string>());
    return true;
}

// Exactly a JSON boolean / integer: no 1 for true, no 90.5 for 90.
bool read_bool(const nlohmann::json& j, const char* key, bool& out)
{
    auto it = j.find(key);
    if (it == j.end()) return true;
    if (!it->is_boolean()) return false;
    out = it->get<bool>();
    return true;
}

// A JSON integer read as 64-bit (an unsigned one above INT64_MAX would wrap
// to a negative) and kept only within +-1000000, so no value is narrowed.
bool small_int(const nlohmann::json& v, int& out)
{
    if (!v.is_number_integer()) return false;
    if (v.is_number_unsigned() && v.get<unsigned long long>() > 1000000ULL) return false;
    const long long n = v.get<long long>();
    if (n < -1000000 || n > 1000000) return false;
    out = (int)n;
    return true;
}

bool read_int(const nlohmann::json& j, const char* key, int& out)
{
    auto it = j.find(key);
    if (it == j.end()) return true;
    return small_int(*it, out);
}

void one_of(std::string& value, std::initializer_list<const char*> allowed, const char* fallback)
{
    for (const char* a : allowed)
        if (value == a) return;
    value = fallback;
}

// "YYYY-MM-DD", digits where they belong.
bool is_date(const std::string& s) { return date::parse_shape(s); }

}   // namespace

void sanitize(Config& c)
{
    bool language_ok = false;
    for (const char* l : LANGUAGES) language_ok |= c.look.language == l;
    if (!language_ok) c.look.language = "system";
    one_of(c.look.theme, { "system", "light", "dark" }, "dark");
    one_of(c.look.text_size, { "auto", "normal", "large" }, "auto");
    one_of(c.look.effects, { "auto", "full", "reduced" }, "auto");
    one_of(c.update_via, { "", "inplace", "auto", "sphaira", "appstore", "manual" }, "");
    one_of(c.dev_builds_order, { "newest", "number" }, "newest");
    one_of(c.library_view, { "grid", "list" }, "grid");
    if (!c.prices_country.empty() && c.prices_country != "none" && !library::is_price_country(c.prices_country)) c.prices_country.clear();
    bool tab_ok = false;
    for (const char* t : START_TABS) tab_ok |= c.start_tab == t;
    if (!tab_ok) c.start_tab = "home";
    bool keep_ok = false;
    for (int k : LOGS_KEEP) keep_ok |= c.logs_keep == k;
    if (!keep_ok) c.logs_keep = 20;
    keep_ok = false;
    for (int k : LOGS_KEEP_DAYS) keep_ok |= c.logs_keep_days == k;
    if (!keep_ok) c.logs_keep_days = 0;
    keep_ok = false;
    for (int k : LOGS_KEEP_MB) keep_ok |= c.logs_keep_mb == k;
    if (!keep_ok) c.logs_keep_mb = 0;
    if (c.connections < 1 || c.connections > 8) c.connections = 4;
    if (c.prefetch < 0 || c.prefetch > 2) c.prefetch = 0;
    std::vector<std::string> sources;
    for (const std::string& u : c.title_sources)
        if (u.starts_with("https://") && u.size() <= 512 && sources.size() < MAX_TITLE_SOURCES &&
            !std::ranges::contains(sources, u))
            sources.push_back(u);
    c.title_sources = std::move(sources);
    if (!is_date(c.update_checked)) c.update_checked.clear();
    if (c.update_latest.size() > 32) c.update_latest.clear();
    if (c.update_snooze_version.size() > 32 || !is_date(c.update_snooze_until))
        c.update_snooze_version.clear(), c.update_snooze_until.clear();
    if (!is_date(c.support_reminded)) c.support_reminded.clear();
    if (c.seen_version.size() > 32) c.seen_version.clear();
    if (c.crash_seen.size() > 64) c.crash_seen.clear();
}

// The file as JSON, or null when it is not a JSON object.
static nlohmann::json parse_object(const std::string& text)
{
    nlohmann::json j(nullptr);
    json_util::parse_with(text, [&j](nlohmann::json parsed) {
        if (parsed.is_object()) j = std::move(parsed);
        return true;
    });
    return j;
}

// There but not readable as settings: defaults, and the file is put aside
// before anything (support_flow, fw_gate save by themselves at start-up)
// writes the defaults over it.
static void damaged()
{
    s_config = Config{};
    s_loaded = paths::put_aside(paths::config_file()) ? Loaded::PutAside : Loaded::Stuck;
}

static void load_shared()
{
    std::string text;
    if (!paths::read_file(paths::config_file(), text)) {
        if (paths::exists_or_pending(paths::config_file())) damaged();   // missing: a first run, silently
        return;
    }
    const nlohmann::json j = parse_object(text);
    if (j.is_null()) {
        damaged();
        return;
    }
    // A newer build's file: defaults in memory, the file left as it is.
    const auto schema = j.find("schema");
    if (schema != j.end() && schema->is_number_integer() &&
        (schema->is_number_unsigned() ? schema->get<unsigned long long>() > (unsigned)SCHEMA : schema->get<long long>() > SCHEMA)) {
        s_loaded = Loaded::Newer;
        return;
    }
    s_loaded = Loaded::Read;
    s_file = j;

    Config& c = s_config;
    read_string(j, "language", c.look.language);
    read_string(j, "theme", c.look.theme);
    read_string(j, "theme_file", c.look.theme_file);
    read_string(j, "text_size", c.look.text_size);
    read_string(j, "effects", c.look.effects);
    read_bool(j, "dev_mode", c.dev_mode);
    read_bool(j, "frame_stats", c.frame_stats);
    read_bool(j, "cadran_glow", c.look.glow);
    read_string(j, "update_via", c.update_via);
    read_bool(j, "update_daily", c.update_daily);
    read_string(j, "update_checked", c.update_checked);
    read_string(j, "update_latest", c.update_latest);
    read_string(j, "update_snooze_version", c.update_snooze_version);
    read_string(j, "update_snooze_until", c.update_snooze_until);
    read_string(j, "start_tab", c.start_tab);
    read_int(j, "logs_keep", c.logs_keep);
    read_int(j, "logs_keep_days", c.logs_keep_days);
    read_int(j, "logs_keep_mb", c.logs_keep_mb);
    read_bool(j, "intro", c.intro);
    read_string(j, "dev_builds_order", c.dev_builds_order);
    read_string(j, "library_view", c.library_view);
    read_bool(j, "support_reminder", c.support_reminder);
    read_bool(j, "notices_others", c.notices_others);
    read_bool(j, "keep_awake", c.keep_awake);
    read_int(j, "connections", c.connections);
    read_int(j, "prefetch", c.prefetch);
    read_strings(j, "title_sources", c.title_sources);
    read_bool(j, "nlib_api", c.nlib_api);
    read_bool(j, "popular_online", c.popular_online);
    read_string(j, "prices_country", c.prices_country);
    read_string(j, "support_reminded", c.support_reminded);
    read_string(j, "seen_version", c.seen_version);
    read_string(j, "crash_seen", c.crash_seen);
}

void load()
{
    s_config = Config{};
    s_loaded = Loaded::Missing;
    s_file = nlohmann::json::object();
    load_shared();
    sanitize(s_config);
}

bool write(const Snapshot& snap)
{
    return paths::atomic_write(paths::config_file(), snap.shared);
}

bool snapshot(Snapshot& snap)
{
    // It would overwrite the unreadable file, or a newer build's.
    if (s_loaded == Loaded::Stuck || s_loaded == Loaded::Newer) return false;
    const auto dump = [](const nlohmann::json& j) {
        return j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) + "\n";
    };
    nlohmann::json j = s_file;   // the keys this build does not know, kept
    j["schema"]          = SCHEMA;
    j["language"]        = s_config.look.language;
    j["theme"]           = s_config.look.theme;
    j["theme_file"]      = s_config.look.theme_file;
    j["text_size"]       = s_config.look.text_size;
    j["effects"]         = s_config.look.effects;
    j["dev_mode"]        = s_config.dev_mode;
    j["frame_stats"]     = s_config.frame_stats;
    j["cadran_glow"]     = s_config.look.glow;
    j["update_via"]      = s_config.update_via;
    j["update_daily"]    = s_config.update_daily;
    j["update_checked"]  = s_config.update_checked;
    j["update_latest"]   = s_config.update_latest;
    j["update_snooze_version"] = s_config.update_snooze_version;
    j["update_snooze_until"]   = s_config.update_snooze_until;
    j["start_tab"]       = s_config.start_tab;
    j["logs_keep"]       = s_config.logs_keep;
    j["logs_keep_days"]  = s_config.logs_keep_days;
    j["logs_keep_mb"]    = s_config.logs_keep_mb;
    j["intro"]           = s_config.intro;
    j["dev_builds_order"] = s_config.dev_builds_order;
    j["library_view"] = s_config.library_view;
    j["support_reminder"] = s_config.support_reminder;
    j["notices_others"]   = s_config.notices_others;
    j["keep_awake"] = s_config.keep_awake;
    j["connections"] = s_config.connections;
    j["prefetch"] = s_config.prefetch;
    j["title_sources"] = s_config.title_sources;
    j["nlib_api"] = s_config.nlib_api;
    j["popular_online"] = s_config.popular_online;
    j["prices_country"] = s_config.prices_country;
    j["support_reminded"] = s_config.support_reminded;
    j["seen_version"]    = s_config.seen_version;
    j["crash_seen"]      = s_config.crash_seen;
    snap.shared = dump(j);
    return true;
}

}   // namespace config
