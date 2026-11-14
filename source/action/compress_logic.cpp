// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/compress_logic.hpp"

namespace compress_logic
{
namespace
{
bool patched(patches::Module m)
{
    return m == patches::Module::SysPatch || m == patches::Module::File;
}
}   // namespace

std::string signature_blocker(const patches::Report& report)
{
    const bool ldr = patched(report.ldr), fs = patched(report.fs);
    if (ldr && fs) return {};
    const char* what = !ldr && !fs ? "The loader and the file system are not patched"
                       : !ldr      ? "The loader is not patched"
                                   : "The file system is not patched";
    return std::string(what) + " (sys-patch): the compressed copy cannot carry Nintendo's signature and would not start.";
}

}   // namespace compress_logic
