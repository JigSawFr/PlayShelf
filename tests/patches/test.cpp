// Host tests for source/util/patches.cpp: sys-patch log parsing, stale logs,
// sigpatch file detection, the overall verdict and an emuMMC set up on the
// card, on fake SD card trees.
#include "check.h"
#include "temp_dir.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/paths.hpp"
#include "util/patches.hpp"

using namespace patches;

static std::string g_base;
static int g_n = 0;

static std::string fresh_root()
{
    std::string root = g_base + "/sd" + std::to_string(g_n++);
    CHECK(paths::ensure_dir(root));
    return root;
}

static void put(const std::string& root, const std::string& rel, const std::string& content = "x")
{
    CHECK(paths::atomic_write(root + "/" + rel, content));
}

static const char* LOG_OK =
    "[fs]\n"
    "noncasigchk_old=Unpatched\n"
    "noncasigchk_new=Patched (sys-patch) (0x7A1B0)\n"
    "nocntchk=Patched (sys-patch) (0x8C2D4)\n"
    "[ldr]\n"
    "noacidsigchk=Patched (sys-patch) (0x6E8)\n"
    "[es]\n"
    "es1=Unpatched\n"
    "es2=Patched (file)\n"
    "[nifm]\n"
    "ctest=Skipped\n"
    "[stats]\n"
    "version=v1.5.9\n"
    "fw_version=23.0.1\n"
    "is_emummc=1\n";

static void install_syspatch(const std::string& root, bool at_boot)
{
    put(root, "atmosphere/contents/420000000000000B/exefs.nsp");
    if (at_boot) put(root, "atmosphere/contents/420000000000000B/flags/boot2.flag", "");
}

int main()
{
    TempDir tmp("patches");
    g_base = tmp.path();

    // Parser: sections, comments, blank lines, spaces, CRLF, no trailing newline.
    {
        Ini ini = parse_ini("; comment\r\n[a]\r\n k = v w \r\n\r\n[b]\nx=1\ny");
        CHECK(ini["a"]["k"] == "v w" && ini["b"]["x"] == "1" && ini["b"].count("y") == 0);
        CHECK(parse_ini("").empty());
    }

    // Module aggregation.
    {
        Ini ini = parse_ini(LOG_OK);
        CHECK(module_state(ini, "fs") == Module::SysPatch);
        CHECK(module_state(ini, "ldr") == Module::SysPatch);
        CHECK(module_state(ini, "es") == Module::File);
        CHECK(module_state(ini, "nifm") == Module::Skipped);
        CHECK(module_state(ini, "nim") == Module::Missing);
        Ini other = parse_ini("[a]\np=Unpatched\nq=Disabled\n[b]\np=Disabled\n[c]\np=Failed (svcWriteDebugProcessMemory)\nq=Unpatched\n");
        CHECK(module_state(other, "a") == Module::Unpatched);
        CHECK(module_state(other, "b") == Module::Disabled);
        CHECK(module_state(other, "c") == Module::Failed);
    }

    // Patterns named with their firmware range, as sys-patch names them.
    {
        CHECK(pattern_for("nocntchk_19.0.0+", "23.0.1"));
        CHECK(pattern_for("es_19.0.0-21.2.0", "21.2.0") && pattern_for("es_19.0.0-21.2.0", "19.0.0"));
        CHECK(!pattern_for("es_19.0.0-21.2.0", "22.0.0"));
        CHECK(!pattern_for("noncasigchk_4.0.0-16.1.0", "23.0.1"));
        CHECK(pattern_for("noncasigchk_17.0.0+", "17.0.0") && !pattern_for("noncasigchk_17.0.0+", "16.1.0"));
        CHECK(!pattern_for("noncasigchk_new", "23.0.1") && !pattern_for("noacidsigchk", "23.0.1"));
        CHECK(!pattern_for("nocntchk_19.0.0+", "") && !pattern_for("x_1.0-2.0", "1.5.0"));
    }

    // One pattern patched, another meant for this firmware not: the check
    // it removes stays (a forwarder "could not be started").
    {
        const char* log =
            "[fs]\n"
            "noacidsigchk_1.0.0-9.2.0=Skipped\n"
            "noncasigchk_4.0.0-16.1.0=Unpatched\n"
            "noncasigchk_17.0.0+=Patched (sys-patch)\n"
            "nocntchk_1.0.0-18.1.0=Unpatched\n"
            "nocntchk_19.0.0+=Unpatched\n"
            "[ldr]\n"
            "noacidsigchk_10.0.0+=Failed (svcWriteDebugProcessMemory)\n"
            "[es]\n"
            "es_19.0.0-21.2.0=Unpatched\n"
            "es_22.0.0+=Patched (sys-patch)\n"
            "[stats]\nversion=v1.6.2\nfw_version=23.0.1\nis_emummc=1\n";
        Ini ini = parse_ini(log);
        std::vector<std::string> gaps;
        CHECK(module_state(ini, "fs", "23.0.1", &gaps) == Module::Partial);
        CHECK((gaps == std::vector<std::string>{ "fs:nocntchk_19.0.0+=Unpatched" }));
        CHECK(module_state(ini, "fs") == Module::SysPatch);   // no firmware: any pattern
        CHECK(module_state(ini, "es", "23.0.1") == Module::SysPatch);
        CHECK(module_state(ini, "ldr", "23.0.1") == Module::Failed);

        std::string root = fresh_root();
        install_syspatch(root, true);
        put(root, "config/sys-patch/log.ini", log);
        Report r = detect(root, "23.0.1", true);
        CHECK(r.log_version == "v1.6.2" && r.fs == Module::Partial && !patched(r.fs));
        CHECK(issue(r) == Issue::NotPatched && status(r) == Status::SysPatchIncomplete);
        CHECK((unpatched(r) == std::vector<std::string>{ "fs", "ldr" }));
        CHECK((r.gaps == std::vector<std::string>{ "fs:nocntchk_19.0.0+=Unpatched" }));
        CHECK(std::string(module_name(r.fs)) == "partial");

        // A pattern turned off in config.ini, meant for this firmware: a gap too.
        put(root, "config/sys-patch/log.ini",
            "[fs]\nnoncasigchk_17.0.0+=Patched (sys-patch)\nnocntchk_19.0.0+=Disabled\n"
            "[ldr]\nnoacidsigchk_10.0.0+=Patched (sys-patch)\n[es]\nes_22.0.0+=Patched (file)\n"
            "[stats]\nfw_version=23.0.1\nis_emummc=1\n");
        r = detect(root, "23.0.1", true);
        CHECK(r.fs == Module::Partial && r.ldr == Module::SysPatch && r.es == Module::File);
        CHECK((r.gaps == std::vector<std::string>{ "fs:nocntchk_19.0.0+=Disabled" }));
    }

    // Nothing at all.
    {
        Report r = detect(fresh_root(), "23.0.1", true);
        CHECK(!r.syspatch_installed && r.files.empty() && !r.log_present);
        CHECK(status(r) == Status::None && issue(r) == Issue::None);
    }

    // Sigpatch files only (Atmosphère folders + Hekate), upper-case extension too.
    {
        std::string root = fresh_root();
        put(root, "atmosphere/exefs_patches/es_patches/ABCDEF.ips");
        put(root, "atmosphere/kip_patches/fs_patches/0123.IPS");
        put(root, "atmosphere/kip_patches/loader_patches/readme.txt");   // not a patch
        put(root, "bootloader/patches.ini");
        Report r = detect(root, "23.0.1", false);
        CHECK((r.files == std::vector<std::string>{ "es", "fs", "hekate" }));
        CHECK(status(r) == Status::FilesOnly);
    }

    // sys-patch working on this boot (emuMMC), plus leftover files.
    {
        std::string root = fresh_root();
        install_syspatch(root, true);
        put(root, "config/sys-patch/log.ini", LOG_OK);
        put(root, "atmosphere/exefs_patches/es_patches/ABCDEF.ips");
        Report r = detect(root, "23.0.1", true);
        CHECK(r.log_present && r.log_current && r.log_fw == "23.0.1" && r.log_emummc);
        CHECK(status(r) == Status::SysPatch && issue(r) == Issue::None);
        CHECK(r.files.size() == 1 && unpatched(r).empty());

        // Same log seen from sysMMC, or after a firmware update: stale.
        r = detect(root, "23.0.1", false);
        CHECK(!r.log_current && issue(r) == Issue::StaleLog && status(r) == Status::SysPatchIncomplete);
        r = detect(root, "23.1.0", true);
        CHECK(issue(r) == Issue::StaleLog);
    }

    // Installed but not started at boot.
    {
        std::string root = fresh_root();
        install_syspatch(root, false);
        put(root, "config/sys-patch/log.ini", LOG_OK);
        Report r = detect(root, "23.0.1", true);
        CHECK(issue(r) == Issue::NotAtBoot && status(r) == Status::SysPatchIncomplete);
    }

    // No log (logging off, or never ran); empty log counts as none.
    {
        std::string root = fresh_root();
        install_syspatch(root, true);
        CHECK(issue(detect(root, "23.0.1", true)) == Issue::NoLog);
        put(root, "config/sys-patch/log.ini", "");
        CHECK(issue(detect(root, "23.0.1", true)) == Issue::NoLog);
    }

    // Turned off for this storage (patch_sysmmc=0).
    {
        std::string root = fresh_root();
        install_syspatch(root, true);
        put(root, "config/sys-patch/log.ini",
            "[fs]\na=Skipped\n[ldr]\nb=Skipped\n[es]\nc=Skipped\n[stats]\nfw_version=23.0.1\nis_emummc=0\n");
        Report r = detect(root, "23.0.1", false);
        CHECK(issue(r) == Issue::Skipped && status(r) == Status::SysPatchIncomplete);
    }

    // A module left unpatched (firmware newer than sys-patch's patterns).
    {
        std::string root = fresh_root();
        install_syspatch(root, true);
        put(root, "config/sys-patch/log.ini",
            "[fs]\na=Patched (sys-patch)\n[ldr]\nb=Patched (sys-patch)\n[es]\nc=Unpatched\nd=Failed (svcWriteDebugProcessMemory)\n"
            "[stats]\nfw_version=23.0.1\nis_emummc=0\n");
        Report r = detect(root, "23.0.1", false);
        CHECK(issue(r) == Issue::NotPatched);
        CHECK((unpatched(r) == std::vector<std::string>{ "es" }));
        CHECK(r.es == Module::Failed);
    }

    // sigpatch files beside sys-patch: mixed, and set aside (moved, not
    // deleted; hekate's patches.ini and the nifm patches stay).
    {
        std::string root = fresh_root();
        install_syspatch(root, true);
        put(root, "config/sys-patch/log.ini", LOG_OK);
        CHECK(!mixed(detect(root, "23.0.1", true)));
        put(root, "atmosphere/exefs_patches/nfim_ctest/A.ips");
        put(root, "bootloader/patches.ini");
        CHECK(!mixed(detect(root, "23.0.1", true)));   // nifm and hekate's: not sigpatches beside it
        put(root, "atmosphere/kip_patches/fs_patches/F.ips");
        put(root, "atmosphere/kip_patches/loader_patches/L.ips");
        put(root, "atmosphere/exefs_patches/es_patches/E.ips");
        Report r = detect(root, "23.0.1", true);
        CHECK(mixed(r) && can_set_aside(r));
        CHECK(!can_set_aside(detect(root, "23.0.1", false)));   // its log from the other system
        std::vector<std::string> moved;
        std::string err;
        CHECK(set_aside(root, root + "/aside", &moved, &err));
        CHECK(moved.size() == 3);
        CHECK(paths::file_exists(root + "/aside/atmosphere/kip_patches/fs_patches/F.ips"));
        CHECK(paths::file_exists(root + "/aside/atmosphere/exefs_patches/es_patches/E.ips"));
        CHECK(paths::file_exists(root + "/atmosphere/exefs_patches/nfim_ctest/A.ips"));
        CHECK(paths::file_exists(root + "/bootloader/patches.ini"));
        r = detect(root, "23.0.1", true);
        CHECK(!mixed(r) && status(r) == Status::SysPatch);
        // Nothing left: nothing moved. Again with files: never over the first.
        moved.clear();
        CHECK(set_aside(root, root + "/aside", &moved, &err) && moved.empty());
        put(root, "atmosphere/kip_patches/fs_patches/G.ips");
        CHECK(!set_aside(root, root + "/aside", &moved, &err) && !err.empty());
        CHECK(paths::file_exists(root + "/atmosphere/kip_patches/fs_patches/G.ips"));
        // Without sys-patch, files are the patches: never mixed.
        std::string bare = fresh_root();
        put(bare, "atmosphere/kip_patches/fs_patches/F.ips");
        CHECK(!mixed(detect(bare, "23.0.1", true)));
    }

    // An emuMMC set up on the card: only [emummc] enabled other than 0 counts.
    {
        std::string root = fresh_root();
        CHECK(!emummc_configured(root));
        put(root, "emummc/emummc.ini", "[emummc]\nenabled=0\nsector=0x0\npath=emuMMC/SD00\n");
        CHECK(!emummc_configured(root));
        put(root, "emummc/emummc.ini", "[other]\nenabled=1\n");
        CHECK(!emummc_configured(root));
        put(root, "emummc/emummc.ini", "[emummc]\r\nenabled = 1\r\nsector=0x0\r\npath=emuMMC/SD00\r\n");
        CHECK(emummc_configured(root));
    }

    CHECK(tmp.remove());
    return CHECK_DONE("patches detection assertions passed");
}
