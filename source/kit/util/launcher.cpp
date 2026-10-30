// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/launcher.hpp"

#include "core/platform.h"
#include "util/paths.hpp"

namespace launcher
{

namespace
{
// Where each store installs itself. sphaira looks for its own copy in the
// first two (sphaira/source/app.cpp); hb-appstore unzips to /switch/appstore/.
const char* const SPHAIRA[]  = { "/switch/sphaira/sphaira.nro", "/switch/sphaira.nro" };
const char* const APPSTORE[] = { "/switch/appstore/appstore.nro" };
// libget's folder for PlayShelf: hb-appstore runs from /switch/appstore/
// with its records in ./.get/packages/; sphaira's App Store writes the same
// one for the official catalogue (sphaira's ui/menus/appstore.cpp).
const char* const STORE_RECORD = "/switch/appstore/.get/packages/PlayShelf/";

bool exists(const std::string& sd_path)
{
    std::string root = paths::sd_root();
    if (!root.empty() && root.back() == '/') root.pop_back();
    return paths::file_exists(root + sd_path);
}

template <size_t N>
Target first_existing(Store store, const char* const (&candidates)[N])
{
    for (const char* p : candidates)
        if (exists(p)) return Target{ store, p };
    return Target{};
}
}   // namespace

Target find(const std::string& preference)
{
    if (preference == "manual" || preference == "inplace") return Target{};
    if (preference == "sphaira") return first_existing(Store::Sphaira, SPHAIRA);
    if (preference == "appstore") return first_existing(Store::AppStore, APPSTORE);
    Target t = first_existing(Store::Sphaira, SPHAIRA);
    return t.store != Store::None ? t : first_existing(Store::AppStore, APPSTORE);
}

bool installed_by_store()
{
    return exists(std::string(STORE_RECORD) + "manifest.install");
}

bool can_launch()
{
    return platform_can_launch();
}

bool launch(const Target& target)
{
    if (target.store == Store::None) return false;
    return launch_nro(target.path);
}

bool launch_nro(const std::string& path)
{
    if (path.empty() || path[0] != '/') return false;
    return platform_set_next_load(("sdmc:" + path).c_str());
}

bool launch_nro_args(const std::string& path, const std::string& args)
{
    if (path.rfind("sdmc:/", 0) != 0 || args.empty()) return false;
    return platform_set_next_load_args(path.c_str(), args.c_str());
}

}   // namespace launcher
