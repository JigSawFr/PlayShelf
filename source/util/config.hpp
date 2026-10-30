// config — small persistent settings in sd:/config/playshelf/config.json.
// Every key, its values and default: docs/config.md (keep it in step).
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <iterator>
#include <string>
#include <vector>

#include <aurore/cadran/ui/prefs.hpp>

namespace config
{

// The title database's default sources, in their order: CyberFoil-DB's
// manifest (names, icons, packs checked by their SHA-256), then NXMeta's
// Tinfoil index (names, publishers, dates) and Ghostland's title list
// (names).
constexpr const char* CYBERFOIL_TITLE_SOURCE =
    "https://github.com/luketanti/CyberFoil-DB/releases/latest/download/offline_db_manifest.json";
constexpr const char* NXMETA_TITLE_SOURCE = "https://nx-meta.nlib.cc/index.json";
constexpr const char* GHOSTLAND_TITLE_SOURCE = "https://data.ghostland.at/nx/owned.json";
constexpr const char* DEFAULT_TITLE_SOURCES[] = { CYBERFOIL_TITLE_SOURCE, NXMETA_TITLE_SOURCE, GHOSTLAND_TITLE_SOURCE };
constexpr size_t MAX_TITLE_SOURCES = 8;

struct Config
{
    // The look Aurore's Cadran draws (ui::use_prefs): the language, the
    // theme and its palette (a preset or a theme file, docs/themes.md), the text size ("auto": larger while docked), the effects ("auto":
    // lighter off the dock) and the screens' background glow (Developer ›
    // glow).
    ui::Prefs look;
    bool dev_mode = false;             // developer tools (7 presses on About > Version)
    bool frame_stats = false;          // Developer › frame statistics (action/frame_stats_flow)
    // How an update is installed: "" (not chosen yet: asked on the first
    // update), "inplace" (PlayShelf downloads it and replaces itself),
    // "auto", "sphaira", "appstore", "manual".
    std::string update_via;
    bool update_daily = false;         // check for updates at start-up, once a day
    std::string update_checked;        // "YYYY-MM-DD" of the last check, any kind
    // The latest release the last check found ("1.2.0"): a newer one than the
    // running version is offered at each start-up (action/update_flow) until
    // installed or postponed. update_snooze_version / _until: "Later" on that
    // screen, for that version until that day ("YYYY-MM-DD", 30 days at most).
    std::string update_latest;
    std::string update_snooze_version;
    std::string update_snooze_until;
    std::string start_tab = "home";   // tab shown at start-up (main.xml order: START_TABS)
    // The saved reports and the crash files kept, each folder on its own
    // (util/log_files.hpp; 0: no limit): the newest logs_keep, those of the
    // last logs_keep_days days, the newest that fit in logs_keep_mb MB.
    int  logs_keep       = 20;
    int  logs_keep_days  = 0;
    int  logs_keep_mb    = 0;
    bool intro = true;                 // the loading screen stays 2 s at start-up (activity/loading_activity.hpp)
    std::string dev_builds_order = "newest";   // Install another build's pull requests: "newest" or "number"
    std::string library_view = "grid";         // the Library's games: "grid" (covers) or "list" (Ⓨ switches)

    // "Support PlayShelf" once a month at start-up (Preferences; off stays
    // off, updates included), and "What's new" once after each update. See
    // util/support.hpp. support_reminded: "YYYY-MM-DD" of the last reminder
    // (or of when the month started); seen_version: the version whose
    // "What's new" was handled.
    bool        support_reminder = true;
    bool        notices_others   = true;   // the other Boréale homebrews' notices in the bell, counted on it
    bool        look_shared      = true;   // the look shared with the other Boréale homebrews (action/shared_look)
    bool        keep_awake = true;     // the console does not go to sleep while downloads run (action/download_flow)
    int         connections = 4;       // a download's connections at once (install/split_stream), 1 to 8
    int         prefetch = 0;          // packages fetched ahead while one installs (action/download_flow), 0 to 2
    // The title databases' manifests (action/titledb_flow), the first that
    // knows a title naming it: HTTPS addresses, MAX_TITLE_SOURCES at most;
    // DEFAULT_TITLE_SOURCES unless changed, none for no database.
    std::vector<std::string> title_sources{ std::begin(DEFAULT_TITLE_SOURCES), std::end(DEFAULT_TITLE_SOURCES) };
    // Nlib API (titledb/nlib) asked, one game at a time, for the icons
    // neither a server nor the title databases have (action/library_icons)
    // and for a game's page: its details, banner, screenshots (action/nlib_flow).
    bool nlib_api = true;
    // Home's popular games: popular.json fetched again once a day (else the
    // one PlayShelf came with).
    bool popular_online = true;
    std::string support_reminded;
    std::string seen_version;
    // The newest crash file (logs/crashes/) already said: a newer one means
    // PlayShelf closed unexpectedly since.
    std::string crash_seen;
};

// "schema" in config.json (none counts as 1): one more when a key changes
// meaning. The keys this build does not know are written back as they were.
constexpr int    SCHEMA             = 1;
// The languages PlayShelf is translated into, after "system" (the console's
// own): Aurore's Cadran's (ui::LANGUAGES), each with
// resources/i18n/<code>/playshelf.json here. tools/check_resources.py
// checks it.
inline constexpr const auto& LANGUAGES = ui::LANGUAGES;
// What theme may be, in the pickers' order: the console's own, then the two
// forced ones. Dark unless chosen otherwise (ui::Prefs::theme).
inline constexpr const auto& THEMES = ui::THEMES;
// What text_size may be: the default first.
inline constexpr const auto& TEXT_SIZES = ui::TEXT_SIZES;
// What effects may be: the default first.
inline constexpr const auto& EFFECTS = ui::EFFECTS;
// What start_tab may be, in the sidebar's order.
constexpr const char* START_TABS[] = { "home", "library", "catalogue", "preferences", "about" };
// What About › Help & diagnostics offers for the reports and crash files kept.
constexpr int LOGS_KEEP[]      = { 0, 5, 10, 20, 50 };
constexpr int LOGS_KEEP_DAYS[] = { 0, 7, 30, 90, 365 };
constexpr int LOGS_KEEP_MB[]   = { 0, 1, 5, 20 };

// What the last load() found. A config.json that is there but cannot be read
// or parsed (not one wrong field: those keep their default alone) is put
// aside as config.json.bad (paths::put_aside: the previous .bad is kept as
// .bad.1), and the defaults are used. When it cannot be moved, snapshot()
// refuses to write for the rest of the session rather than replace it.
// Newer: a file of a newer schema than SCHEMA (a newer PlayShelf's, before
// going back to this one): the defaults are used and snapshot() refuses to
// write, so the file stays as that build left it.
enum class Loaded { Missing, Read, PutAside, Stuck, Newer };

Config& get();
void    load();          // never throws; missing / broken file => defaults, per field
Loaded  loaded();
// A save in two steps, so the write can be left to another thread: the
// file's content now (false: Loaded::Stuck, nothing may be written), then
// writing it. write() touches no config state: callable from any thread.
struct Snapshot
{
    std::string shared;
};
bool    snapshot(Snapshot& snap);
bool    write(const Snapshot& snap);
// Puts every out-of-range value back to its default (load() calls it).
void    sanitize(Config& c);

}   // namespace config
