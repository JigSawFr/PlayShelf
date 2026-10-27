// pack — CyberFoil-DB's packs, read where they lie on the SD card: the
// titles pack ("CFTITLE1": each title id's name, publisher, newest version,
// release date and size) and the icons pack ("CFICONP1": each one's image,
// WebP in CyberFoil-DB's). Their index is read whole and checked against
// the file (a few MB for every title there is); a name, an icon, read from
// the file when asked. Text is bounded and made safe (no control
// characters, valid UTF-8), so a pack's cannot add lines to a screen.
//
// Plain C++ for the host tests (tests/titledb).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace titledb
{

struct Title
{
    std::string name, publisher;
    std::optional<uint32_t> version;   // a title's newest (an update's: the newest update)
    uint32_t    release_date = 0;      // YYYYMMDD, 0 when unknown
    uint64_t    size = 0;              // 0 when unknown
};

class TitlePack
{
  public:
    TitlePack() = default;
    ~TitlePack();
    TitlePack(const TitlePack&) = delete;
    TitlePack& operator=(const TitlePack&) = delete;

    // False (why set) for a file that is not a titles pack, or whose index
    // does not hold together.
    bool   open(const std::string& path, std::string* why);
    size_t count() const { return this->index.size(); }
    // `id`'s entry, nothing when the pack has none (or nothing of it).
    std::optional<Title> find(uint64_t id) const;
    // The base games released between `from` and `to` (YYYYMMDD, both
    // included), added to `out` up to `max` of them, in title id order.
    void released(uint32_t from, uint32_t to, size_t max, std::vector<uint64_t>* out) const;
    // Each base game's id and name (those it names), by id, until `each`
    // answers false.
    void each_base_name(const std::function<bool(uint64_t id, const std::string& name)>& each) const;

  private:
    struct Entry
    {
        uint64_t id;
        uint32_t name, publisher;   // offsets in the strings, 0: none
        uint64_t size;
        uint32_t version, release_date, flags;
    };
    std::string read_string(uint32_t offset) const;

    std::vector<Entry> index;   // by title id
    uint64_t           strings = 0, file_size = 0;
    mutable FILE*      file = nullptr;
    mutable std::mutex lock;
};

class IconPack
{
  public:
    IconPack() = default;
    ~IconPack();
    IconPack(const IconPack&) = delete;
    IconPack& operator=(const IconPack&) = delete;

    bool   open(const std::string& path, std::string* why);
    size_t count() const { return this->index.size(); }
    bool   has(uint64_t id) const;
    // `id`'s image and its kind ("webp", "png", "jpg"); false for none.
    bool find(uint64_t id, std::vector<uint8_t>* bytes, std::string* ext) const;

    static constexpr uint32_t MAX_ICON = 1u << 20;   // an icon's bytes, at most

  private:
    struct Entry
    {
        uint64_t id, offset;
        uint32_t size;
        char     ext[8];
    };
    const Entry* entry_of(uint64_t id) const;

    std::vector<Entry> index;   // by title id
    uint64_t           data = 0;
    mutable FILE*      file = nullptr;
    mutable std::mutex lock;
};

// A pack's text made safe: control characters spaces, at most `max` bytes
// (cut on a character), "" when it is not UTF-8.
std::string safe_text(const std::string& text, size_t max = 256);

}   // namespace titledb
