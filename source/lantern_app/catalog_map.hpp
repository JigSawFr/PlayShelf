// A Lantern server's apps and sections as the neutral library::Catalog the
// tabs read: an app becomes an item (its title id, version, kind, size and
// the address of its file), a section keeps its apps' order. Names are
// bounded and made safe for a screen; an app whose file is not https://, or
// whose title id is not 16 hex digits, is counted in `skipped` and left out.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <map>
#include <span>
#include <string>

#include <lantern/types.hpp>

#include "library/catalog.hpp"

namespace lantern_app
{

// The index in Catalog::items of each app, by the app's id.
using AppIndex = std::map<std::string, size_t>;

// Adds `apps` to `catalog`; `origin` makes a relative `file` absolute.
void add_apps(library::Catalog& catalog, AppIndex& index, std::span<const lantern::App> apps, const std::string& origin);

// The sections of `sections` over the items already in `catalog`; an app id
// no item has is left out.
void add_sections(library::Catalog& catalog, const AppIndex& index, std::span<const lantern::Section> sections);

}   // namespace lantern_app
