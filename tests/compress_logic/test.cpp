// action/compress_logic.cpp: a game is compressed in place only when the
// loader and the file system are both patched.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <string>

#include "action/compress_logic.hpp"
#include "check.h"

using patches::Module;

static patches::Report report(Module ldr, Module fs)
{
    patches::Report r;
    r.ldr = ldr;
    r.fs  = fs;
    return r;
}

int main()
{
    for (Module ldr : {Module::SysPatch, Module::File})
        for (Module fs : {Module::SysPatch, Module::File})
            CHECK(compress_logic::signature_blocker(report(ldr, fs)).empty());

    // Every other state of either module blocks, and the reason names it.
    for (Module bad : {Module::Missing, Module::Failed, Module::Skipped, Module::Disabled, Module::Unpatched, Module::Partial}) {
        const std::string fs_only  = compress_logic::signature_blocker(report(Module::SysPatch, bad));
        const std::string ldr_only = compress_logic::signature_blocker(report(bad, Module::SysPatch));
        const std::string both     = compress_logic::signature_blocker(report(bad, bad));
        CHECK(fs_only.find("file system") != std::string::npos && fs_only.find("loader") == std::string::npos);
        CHECK(ldr_only.find("loader") != std::string::npos && ldr_only.find("file system") == std::string::npos);
        CHECK(both.find("loader and the file system") != std::string::npos);
    }
    CHECK_DONE("compress_logic");
}
