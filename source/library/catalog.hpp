// catalog — what a library server offers, from its answer: the AeroFoil and
// older Ownfoil API (/api/remote, or /api/shop on older servers: sections of
// items, each with its address, size, title id, version, type and icon),
// Ownfoil 2's GraphQL API (its owned apps, a page at a time, and the
// server's handshake: its name, message of the day and features) and
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
    bool                           truncated = false;   // the server says a section holds more than it sent
    bool                           frozen = false;      // AeroFoil: the account is frozen (its placeholder items)
};

// What Ownfoil says of itself (its OPTIONS handshake on "/").
struct ServerInfo
{
    std::string name, version, motd;
    int         protocol = 0;
    bool        public_shop = false;
    bool        save_backup = false;   // its save backups (not served yet by Ownfoil 2.5)
};

constexpr size_t MAX_ITEMS = 100000;
constexpr int    MAX_DEPTH = 16;

// The API's answer (/api/remote or /api/shop). An "error" it holds, or no
// "sections", fails with that reason (the server's words, bounded).
std::optional<Catalog> parse_api(const std::string& body, const std::string& base_url, std::string* why = nullptr);

// Ownfoil's handshake; nothing when it is not one (no "uid", "name").
std::optional<ServerInfo> parse_handshake(const std::string& body);

// The GraphQL query for a page of Ownfoil's owned apps (`page` from 1), as
// the "query" parameter of a GET to /api/graphql, URL-encoded.
std::string graphql_apps_query(int page, int page_size);
constexpr int GRAPHQL_PAGE = 500;
constexpr int GRAPHQL_MAX_PAGES = 200;   // MAX_ITEMS at most
// A page of the answer added to `c`: its apps that have a download, their
// game's name and icon. *total: how many apps the server has in all. False,
// with why, when it is no answer to that query (or a GraphQL "errors" one).
bool add_graphql_apps(Catalog& c, const std::string& body, const std::string& base_url, uint64_t* total,
                      std::string* why = nullptr);

// "a b&c" as a URL's query value: "a%20b%26c".
std::string url_encode(const std::string& s);

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
