// catalog — what a Lantern server offers, as the tabs read it: items (a
// package: its address, size, title id, version, kind and SHA-256), sections
// of them, the message of the day. One neutral model, filled by
// lantern_app/catalog_map from the server's apps and sections.
//
// Every address is made absolute against the server's and kept only when it
// is https:// (no package over plain HTTP). Strings are bounded, control
// characters made spaces, so a server's text cannot add lines to a screen.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
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
    std::string section;           // the section's id ("" for an index)
    std::string sha256;            // lower-case hex of the file as the server serves it ("" when unknown)
    std::string icon_url;          // the game's icon, https:// ("" for none: the server did not say)
};

struct Section
{
    std::string         id, title;
    std::vector<size_t> items;   // indexes in Catalog::items
};

struct Catalog
{
    std::vector<Item>              items;
    std::vector<Section>           sections;
    std::string                    motd;
    size_t                         skipped = 0;   // items left out (not https, a bad id)
    bool                           truncated = false;   // the server lists more than was read
};

constexpr size_t MAX_ITEMS = 100000;

// `ref` made absolute against `base` (RFC 3986: an absolute URL, "//host",
// "/path", or a path relative to the base's folder, with "." and ".."
// resolved). "" when the result is not https:// or `ref` is not an address.
std::string resolve(const std::string& base, const std::string& ref);

// What a title id is: a base game ends in 000 with bit 12 clear, its update
// in 800, an add-on has bit 12 set.
Kind kind_of(uint64_t title_id);
// The base game a title id belongs to.
uint64_t base_of(uint64_t title_id);

// A server's text for a screen: at most `max` bytes, never cutting a
// UTF-8 sequence, control characters made spaces.
std::string display_text(const std::string& s, size_t max);

}   // namespace library
