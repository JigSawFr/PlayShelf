// catalog — what a library server offers, from its answer: the AeroFoil and
// Ownfoil API (/api/remote, or /api/shop on older servers: sections of
// items, each with its address, size, title id, version, type and icon) and
// the plain JSON index (Tinfoil's custom index without its encryption:
// "files", "directories" of other indexes, "titledb" names, the "success"
// message of the day). One model for both: items, sections, the MOTD, the
// indexes still to read, and the names and versions a titledb gives.
//
// Every address is made absolute against the server's and kept only when it
// is https:// (the plan's rule: no package over plain HTTP); a Google Drive
// address (gdrive:) is left out. The answer is read with a depth limit and
// at most MAX_ITEMS items; strings are bounded, control characters made
// spaces, so a server's text cannot add lines to a screen.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace library
{

enum class Kind : uint8_t { Unknown, Base, Update, Dlc };

struct Item
{
    std::string name;   // shown: the server's, else the file name
    std::string url;    // absolute, https://
    uint64_t    size = 0;
    uint64_t    title_id = 0;   // 0: unknown
    uint32_t    version = 0;
    bool        has_version = false;
    Kind        kind = Kind::Unknown;
    std::string icon_url;          // absolute, https://; "" when none
    uint32_t    release_date = 0;  // YYYYMMDD, 0 when unknown
    std::string section;           // the section's id ("" for an index)
};

struct Section
{
    std::string         id, title;
    std::vector<size_t> items;   // indexes in Catalog::items
};

struct TitleInfo
{
    std::string name;
    uint32_t    version = 0;   // the latest the titledb knows
};

struct Catalog
{
    std::vector<Item>              items;
    std::vector<Section>           sections;
    std::string                    motd;
    std::vector<std::string>       directories;   // other indexes to read (absolute, https://)
    std::map<uint64_t, TitleInfo>  titles;        // from a "titledb"
    size_t                         skipped = 0;   // items left out (not https, Google Drive, malformed)
};

constexpr size_t MAX_ITEMS = 100000;
constexpr int    MAX_DEPTH = 16;

// The API's answer (/api/remote or /api/shop). An "error" it holds, or no
// "sections", fails with that reason (the server's words, bounded).
std::optional<Catalog> parse_api(const std::string& body, const std::string& base_url, std::string* why = nullptr);

// A JSON index. An index without any of its fields fails.
std::optional<Catalog> parse_index(const std::string& body, const std::string& base_url, std::string* why = nullptr);

// `ref` made absolute against `base` (RFC 3986: an absolute URL, "//host",
// "/path", or a path relative to the base's folder, with "." and ".."
// resolved). "" when the result is not https:// or `ref` is not an address.
std::string resolve(const std::string& base, const std::string& ref);

// What a title id is: a base game ends in 000 with bit 12 clear, its update
// in 800, an add-on has bit 12 set.
Kind kind_of(uint64_t title_id);
// The base game a title id belongs to.
uint64_t base_of(uint64_t title_id);

// "Game [0100ABCD00010000][v65536].nsp": the title id and the version
// between brackets, when the name holds them.
bool parse_name_tags(const std::string& name, uint64_t* title_id, uint32_t* version, bool* has_version);

// A server's text for a screen: at most `max` bytes, never cutting a
// UTF-8 sequence, control characters made spaces.
std::string display_text(const std::string& s, size_t max);

}   // namespace library
