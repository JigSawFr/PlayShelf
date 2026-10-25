// Host tests for source/util/diagnostics_logic.cpp: the diagnostic report's
// sizes, the storage's name, the first line of a crash file and the game
// patches' line (nothing found; sigpatch files only; sys-patch at this boot;
// not at boot, its log stale, a pattern left unpatched).
#include "check.h"
#include <string>

#include "util/diagnostics_logic.hpp"

using namespace diagnostic;

static void test_bytes()
{
    CHECK(bytes(0) == "0 KiB" && bytes(1) == "1 KiB" && bytes(1024) == "1 KiB" && bytes(1025) == "2 KiB");
    CHECK(bytes((1ULL << 20) - 1) == "1024 KiB");
    CHECK(bytes(1ULL << 20) == "1.0 MiB" && bytes(327576371) == "312.4 MiB");
    CHECK(bytes(3ULL << 29) == "1.5 GiB" && bytes(UINT64_MAX).find(" GiB") != std::string::npos);
}

static void test_names()
{
    CHECK(std::string(storage_name(SysStorage_EmuMMC)) == "emuMMC");
    CHECK(std::string(storage_name(SysStorage_SysMMC)) == "sysMMC");
    CHECK(storage_name(SysStorage_Unknown) == nullptr);
    CHECK(first_line("Data abort at 0x1234\nmore\n") == "Data abort at 0x1234");
    CHECK(first_line("one line") == "one line" && first_line("").empty() && first_line("\nx").empty());
}

static void test_patch_line()
{
    patches::Report r;
    CHECK(patch_line(r) == "none (issue: none; sys-patch installed=0 boot2=0 log=none; fs=missing ldr=missing es=missing; files=none)");
    r.files = { "es", "fs", "loader" };
    CHECK(patch_line(r) == "sigpatch files only (issue: none; sys-patch installed=0 boot2=0 log=none; fs=missing ldr=missing "
                           "es=missing; files=es,fs,loader)");

    // sys-patch, at this boot, every module patched.
    r = patches::Report();
    r.syspatch_installed = r.syspatch_at_boot = r.log_present = r.log_current = true;
    r.log_fw = "21.0.0";
    r.fs = r.ldr = r.es = patches::Module::SysPatch;
    CHECK(patch_line(r) == "sys-patch (issue: none; sys-patch installed=1 boot2=1 log=current fw=21.0.0 emummc=0 version=?; "
                           "fs=sys-patch ldr=sys-patch es=sys-patch; files=none)");
    // A pattern of this firmware left unpatched.
    r.log_emummc = true;
    r.log_version = "1.5.6";
    r.ldr = patches::Module::Partial;
    r.gaps = { "ldr:noacidsigchk_17.0.0+=Unpatched", "fs:nocntchk_19.0.0+=Unpatched" };
    CHECK(patch_line(r) == "sys-patch incomplete (issue: not patched; sys-patch installed=1 boot2=1 log=current fw=21.0.0 "
                           "emummc=1 version=1.5.6; fs=sys-patch ldr=partial es=sys-patch (not patched: "
                           "ldr:noacidsigchk_17.0.0+=Unpatched, fs:nocntchk_19.0.0+=Unpatched); files=none)");
    // Its log from another boot; not started at boot.
    r.log_current = false;
    CHECK(patch_line(r).find("sys-patch incomplete (issue: stale log;") == 0 && patch_line(r).find("log=stale fw=") != std::string::npos);
    r.syspatch_at_boot = false;
    CHECK(patch_line(r).find("(issue: not at boot; sys-patch installed=1 boot2=0") != std::string::npos);
}

int main()
{
    test_bytes();
    test_names();
    test_patch_line();
    return CHECK_DONE("diagnostics_logic tests passed");
}
