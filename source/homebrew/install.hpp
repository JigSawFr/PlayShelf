// Installing a homebrew package from a Lantern server onto the SD card
// (SPEC.md § 8.3): the library plans it (format, paths, consent, manifest),
// this writes it: a single file at its place, an archive unpacked aside,
// every file checked against its manifest, then moved in; what an update
// no longer ships is removed. The installed ones are kept in a book
// (config/playshelf/homebrew.json, schema 1). Plain C++, host-tested
// (tests/homebrew_install).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <lantern/lantern.hpp>

namespace homebrew
{

struct Record
{
    std::string              id, version, platform, server_uid;
    uint64_t                 build = 0;
    std::vector<std::string> files;   // every path written, from the SD root
};

// What is installed, by id. A file of a newer schema is read as empty and
// never written over.
class Book
{
  public:
    static constexpr size_t MAX_RECORDS = 2000;

    bool load(const std::string& path);
    bool save(const std::string& path) const;
    const Record* find(const std::string& id) const;
    void put(const Record& record);
    void erase(const std::string& id);
    const std::map<std::string, Record>& all() const { return this->records; }
    bool newer = false;   // the file on disk is of a later schema: not written

  private:
    std::map<std::string, Record> records;
};

struct Outcome
{
    bool                     ok = false;
    bool                     declined = false;   // the user refused the paths that need consent
    std::string              why;
    Record                   record;
    std::vector<std::string> removed;   // files of the old version no longer shipped
};

// Asked with the paths that need the user's consent (§ 8.3); false refuses.
using Consent = std::function<bool(const std::vector<std::string>& paths)>;

// Installs `entry` from `package_file` (fetched whole, its SHA-256 checked
// again here) under `sd_root`; `staging` is a folder this may empty and use.
// `old` is the record of the version being replaced, if any.
Outcome install(const lantern::Homebrew& entry, const std::string& package_file, const std::string& sd_root,
                const std::string& staging, const Record* old, const Consent& consent);

// Removes what `record` wrote (a file at a time, then the folders it left
// empty under a "switch/<id>" or "config/<id>" root).
std::vector<std::string> uninstall(const Record& record, const std::string& sd_root);

}   // namespace homebrew
