// insights_index — PlayInsights' index.json (schema 1): each file it
// publishes, the day it was made, its size and the first 16 hex digits of the
// SHA-256 of its bytes. PlayShelf fetches it once a day and downloads a file
// only when its cached copy's hash differs. Plain C++ for the host tests
// (tests/library_index).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace library
{

struct IndexEntry
{
    std::string generated;   // YYYY-MM-DD
    uint64_t    bytes = 0;
    std::string sha256;      // 16 lower-case hex digits
};

class InsightsIndex
{
  public:
    static constexpr size_t   MAX_FILES = 256;
    static constexpr size_t   MAX_NAME = 64;
    static constexpr uint64_t MAX_BYTES = 64ull * 1024 * 1024;

    // From index.json; false when it is not one (the index is left empty).
    // A bad entry (an odd name, no hash, no day) is skipped.
    bool from_json(const std::string& text);
    const IndexEntry* find(const std::string& name) const;
    size_t size() const { return this->files.size(); }

  private:
    std::map<std::string, IndexEntry> files;
};

// The first 16 hex digits of the SHA-256 of `bytes`, as index.json gives them.
std::string short_hash(const std::string& bytes);

// Whether the cached copy of `name` (`cached`: its bytes; "" when there is
// none) must be downloaded again: yes without an index, when the index does
// not list it, or when the hashes differ.
bool must_fetch(const InsightsIndex* index, const std::string& name, const std::string& cached);

}   // namespace library
