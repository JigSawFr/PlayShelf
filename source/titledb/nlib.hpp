// nlib — Nlib API (https://api.nlib.cc/nx/), asked for what no title
// database has: a base game's icon, and for its page its details in the
// language PlayShelf speaks, its banner and its screenshots. Every address
// is built here, never taken from the API's answers (they give http://),
// and asked for one game at a time, the one on screen; an answer is read
// with its text bounded and made safe.
//
// Plain C++ for the host tests (tests/titledb).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace titledb
{

// The size Nlib API is asked for: a cover's, and the database's own.
constexpr int NLIB_ICON_SIZE = 256;
// A game's details: a few KB (a description is the most of it).
constexpr size_t NLIB_MAX_DETAILS = 64 * 1024;
// The screenshots a page shows, at most.
constexpr int NLIB_MAX_SCREENS = 8;

// The icon of the base game `base_id`, `size` pixels square (30 to 4096:
// the API serves the nearest ten); "" for an id that is not a base game's
// (Nlib API knows no update and no add-on) or a size out of range.
std::string nlib_icon_url(uint64_t base_id, int size = NLIB_ICON_SIZE);

// Nlib API's language for PlayShelf's (ui::LANGUAGES: "fr-CA" -> "fr",
// "pt-BR" -> "pt", "zh-Hant" -> "zh"); "en" for one it does not have.
std::string nlib_language(const std::string& locale);

// The base game's details in `language` (nlib_language()), the fields a
// page shows only; "" for an id that is not a base game's.
std::string nlib_details_url(uint64_t base_id, const std::string& language);
// Its banner, 854 × 480 JPEG; its screenshot `n` (1 to NLIB_MAX_SCREENS),
// 1280 × 720 JPEG. "" as nlib_icon_url().
std::string nlib_banner_url(uint64_t base_id);
std::string nlib_screen_url(uint64_t base_id, int n);

struct Details
{
    std::string              name, publisher;
    std::string              release_date;   // "YYYY-MM-DD", "" when unknown
    std::vector<std::string> categories;     // 8 at most
    std::string              intro;          // a line or two
    std::string              description;    // paragraphs, "\n" between lines
    int                      screens = 0;    // 0 to NLIB_MAX_SCREENS
};

// A details answer for `base_id` read: false when it is not one (not
// JSON, another game's, "success": false). Text: control characters
// spaces but the description's and intro's line breaks, each field bounded,
// one not UTF-8 left out.
bool parse_nlib_details(const std::string& json, uint64_t base_id, Details* out);

}   // namespace titledb
