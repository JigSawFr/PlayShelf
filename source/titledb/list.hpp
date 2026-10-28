// list — a title list as some databases publish it, rather than
// CyberFoil-DB's packs: one JSON object, each title id holding what is known
// of it ({"0100000000010000": {"Game Name": "…", "Version": "0", "Size":
// 3751207164}, …}), or a Tinfoil index's "titledb" ({"titledb": {"0100…":
// {"name": "…", "publisher": "…", "releaseDate": "20231220", "version": 0,
// "size": …}}}). Read as a stream (a list of every title is several MB, some
// tens of MB: no tree of it is built) and made a
// titles pack, so it is opened and looked up as CyberFoil-DB's are. No
// icons; no SHA-256 is published for it, so HTTPS alone vouches for it.
//
// Plain C++ for the host tests (tests/titledb).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <istream>
#include <string>
#include <vector>

namespace titledb
{

struct ListEntry
{
    uint64_t    id = 0;
    std::string name, publisher;
    uint32_t    release_date = 0;   // YYYYMMDD
    uint32_t    version = 0;
    bool        has_version = false;
    uint64_t    size = 0;
};

// The entries of a title list, by title id (the last one of an id kept);
// false (why set) for a text that is not one, or that names no title.
bool parse_title_list(std::istream& in, std::vector<ListEntry>* out, std::string* why);
bool parse_title_list(const std::string& text, std::vector<ListEntry>* out, std::string* why);

constexpr size_t MAX_LIST_ENTRIES = 1000000;

// The entries as a titles pack ("CFTITLE1"), names made safe.
std::string titles_pack_of(const std::vector<ListEntry>& entries);

}   // namespace titledb
