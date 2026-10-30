// store — the title databases on the SD card, one folder per source
// (data_dir()/titledb/<its address's hash>/): its packs, each named after
// its SHA-256 ("titles-<16 hex>.pack"), and state.json, which names the
// ones in use. An update fetches the manifest, then each pack it names that
// the folder lacks (a part left by a cut resumed), checks its size and its
// SHA-256 and only then writes state.json: a pack open meanwhile is never
// overwritten, an icons pack the same from one version to the next not
// fetched again. The packs state.json no longer names go afterwards. A
// source that publishes a title list instead (titledb/list): its titles pack
// made from it, in the same folder.
//
// Plain C++ for the host tests (tests/titledb_store).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "titledb/pack.hpp"

namespace titledb
{

// What a source's folder holds (its state.json).
struct State
{
    std::string url, version;
    std::string titles, icons;   // file names in the folder; icons "" for none
    size_t      count = 0;       // the titles pack's entries
    bool        list = false;    // made from a title list (titledb/list), not fetched as a pack
};
// state.json's "schema" (none counts as 1): a newer one's state is read as
// none, and the source fetched again.
constexpr int STATE_SCHEMA = 1;
bool read_state(const std::string& dir, State* out);

// The folder of the source at `url`, under `root`.
std::string source_dir(const std::string& root, const std::string& url);

// What an update needs of the network: the source's address saved to a
// file (a manifest, or a title list: titledb/list; `max` bytes at most), and
// a pack (`size` bytes: those past `resume_from` appended to `path`).
struct Net
{
    std::function<bool(const std::string& url, const std::string& path, uint64_t max,
                       const std::function<void(uint64_t, uint64_t)>& progress, std::string* why)>
        save;
    std::function<bool(const std::string& url, const std::string& path, uint64_t size, uint64_t resume_from,
                       const std::function<void(uint64_t)>& progress, std::string* why)>
        file;
    // The bytes free where the folder is; nothing to say when unset.
    std::function<uint64_t()> free_space;
};

constexpr size_t MAX_TEXT = 64u << 20;

enum class Outcome : uint8_t { Updated, Current, Failed };

// The source at `url` brought to its manifest's version in `dir`.
// `progress(done, total)`: the bytes fetched of those to fetch. Failed: why set.
Outcome update(const std::string& url, const std::string& dir, const Net& net,
               const std::function<void(uint64_t, uint64_t)>& progress, std::string* why);

// The packs in `dir` that its state.json does not name removed, and the
// parts left by a cut with `parts` (an update done: they are older); one
// still open stays, removed next time.
void prune(const std::string& dir, bool parts);

// A source's folder and what it holds, removed (its source taken out of
// the list); false when something stays (a pack still open).
bool remove_source(const std::string& dir);

// The sources opened, in their order: a title's name is the first one's
// that has it, and the rest of what it knows of it comes with it; its icon
// the first one's that has one. A source whose packs do not open is left
// out (why it was, in `skipped`).
class Database
{
  public:
    struct Source
    {
        State                     state;
        std::unique_ptr<TitlePack> titles;
        std::unique_ptr<IconPack>  icons;
    };

    void open(const std::vector<std::string>& dirs);
    std::optional<Title> title(uint64_t id) const;
    // The base games released between `from` and `to` (YYYYMMDD), each once,
    // `max` at most (titledb::TitlePack::released, every source).
    std::vector<uint64_t> released(uint32_t from, uint32_t to, size_t max) const;
    bool                 icon(uint64_t id, std::vector<uint8_t>* bytes, std::string* ext) const;
    const std::vector<Source>& sources() const { return this->list; }
    std::vector<std::string>   skipped;

  private:
    std::vector<Source> list;
};

}   // namespace titledb
