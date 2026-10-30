// patches — how this console patches signature checks for games: the
// sys-patch system module (patches fs / ldr / es in memory at boot, then
// exits) or sigpatch files loaded by Atmosphère / Hekate.
// Plain C++ (no Aurore), so the host tests can run it on a fake SD card.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <map>
#include <string>
#include <vector>

namespace patches
{

// Best result sys-patch logged for one module (all its patterns).
enum class Module
{
    Missing,     // no section in the log
    SysPatch,    // "Patched (sys-patch)"
    File,        // "Patched (file)": already patched by sigpatch files
    Failed,      // "Failed (...)"
    Skipped,     // "Skipped": patching turned off for this storage (patch_sysmmc / patch_emummc)
    Disabled,    // "Disabled": every pattern turned off in config.ini
    Unpatched,   // "Unpatched": no pattern matched this firmware
    Partial,     // patched, but a pattern meant for this firmware was not (see Report::gaps)
};

struct Report
{
    bool syspatch_installed = false;   // atmosphere/contents/420000000000000B/exefs.nsp
    bool syspatch_at_boot   = false;   // .../flags/boot2.flag
    bool log_present        = false;   // config/sys-patch/log.ini
    bool log_current        = false;   // written for this firmware and storage
    std::string log_fw;                // [stats] fw_version
    bool log_emummc         = false;   // [stats] is_emummc
    std::string log_version;           // [stats] version: sys-patch's own
    Module fs  = Module::Missing;
    Module ldr = Module::Missing;
    Module es  = Module::Missing;
    std::vector<std::string> files;    // sigpatch sets found: "es", "fs", "loader", "nifm", "hekate"
    // Patterns of fs, ldr and es meant for this firmware and not patched,
    // as "fs:nocntchk_19.0.0+=Unpatched": a module patched by one of its
    // patterns can still leave a check in place (a forwarder then "could
    // not be started").
    std::vector<std::string> gaps;
};

enum class Status
{
    None,                // nothing detected
    SysPatch,            // sys-patch patched fs, ldr and es at this boot
    SysPatchIncomplete,  // sys-patch installed, but see issue()
    FilesOnly,           // sigpatch files only: sys-patch is the recommended way now
};

enum class Issue
{
    None,
    NotAtBoot,      // installed, no boot2.flag
    NoLog,          // no log.ini (logging turned off, or never ran)
    StaleLog,       // log from another firmware or storage: did not run at this boot
    Skipped,        // patching turned off for this storage
    NotPatched,     // fs, ldr or es left unpatched (see unpatched())
};

using Ini = std::map<std::string, std::map<std::string, std::string>>;

Ini    parse_ini(const std::string& text);
// With `fw` ("23.0.1"), a module whose patterns are not all patched is
// Partial when one left out names a range covering `fw` (sys-patch names
// them so: "noncasigchk_17.0.0+", "nocntchk_1.0.0-18.1.0"); `gaps` gets
// those. A pattern named without a range is an alternative to the others.
Module module_state(const Ini& ini, const std::string& section, const std::string& fw = "",
                    std::vector<std::string>* gaps = nullptr);

// Whether pattern `name` is meant for firmware `fw`: false when its name
// gives no range, or a range without `fw`.
bool pattern_for(const std::string& name, const std::string& fw);

// `root` is the SD card root ("/" on the console). `fw` is "23.0.1".
Report detect(const std::string& root, const std::string& fw, bool emummc);

// An emuMMC is set up on this SD card: [emummc] enabled=1 in emummc/emummc.ini,
// the file fusee reads at boot (hekate writes the same one). From sysMMC, it
// says the other system, with its own parental controls, is one boot away.
bool emummc_configured(const std::string& root);

Status status(const Report& r);
Issue  issue(const Report& r);
std::vector<std::string> unpatched(const Report& r);   // of "fs", "ldr", "es"
bool   patched(Module m);                               // SysPatch or File

const char* module_name(Module m);   // for the diagnostic report

// sigpatch files (.ips for es, fs or the loader) beside sys-patch: two
// tools patching the same checks, which GBAtemp's sys-patch and sigpatch
// threads give as a cause of forwarders that "could not be started".
// hekate's patches.ini alone is not counted (it applies only through
// hekate_ipl.ini's kip1patch lines).
bool mixed(const Report& r);

// The folders of those files, from the SD card root.
constexpr const char* SIGPATCH_DIRS[] = { "atmosphere/kip_patches/fs_patches", "atmosphere/kip_patches/loader_patches",
                                          "atmosphere/exefs_patches/es_patches" };

// Whether set_aside() may run: sys-patch installed, started at boot and
// logged at this boot, so it patches alone once the files are gone.
bool can_set_aside(const Report& r);

// Moves each of SIGPATCH_DIRS that holds .ips files from `root` into
// `dest` (the same path under it), on the same SD card: nothing is
// deleted, and moving them back undoes it. hekate's patches.ini stays
// (hekate_ipl.ini may name it). *moved gets the folders moved. False, with
// a short English reason, when one could not be moved (those moved before
// stay moved, and listed).
[[nodiscard]] bool set_aside(const std::string& root, const std::string& dest, std::vector<std::string>* moved, std::string* error);

}   // namespace patches
