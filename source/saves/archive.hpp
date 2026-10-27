// save_archive — a game's save as a zip archive, the way AeroFoil keeps
// them and CyberFoil makes them: every file of the save under its path in
// it, '/' between folders, an empty save as a single ".empty" entry. Made
// from a folder (the save mounted, core/saves.h) as it is read, deflated,
// each entry's sizes and CRC-32 after its data; read back entry by entry
// (stored or deflated, any zip tool's), each checked against its CRC-32
// and size, its name refused when it would leave the folder. No Zip64:
// AeroFoil takes 4 GB at most. Plain C++ on zlib, host-tested
// (tests/save_archive) and fuzzed (tests/fuzz/save_archive).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace save_archive
{

constexpr uint64_t MAX_TOTAL = 0xFFFFFFFEull;   // the files' bytes, all told
constexpr size_t   MAX_ENTRIES = 65000;
constexpr size_t   MAX_NAME = 512;
constexpr const char* EMPTY_MARK = ".empty";

// Where it is: the files done and their number, the one under way.
using Progress = std::function<void(size_t done, size_t total, const std::string& name)>;

// The zip of every file and folder under `dir`, written to `zip_path`.
bool zip_dir(const std::string& dir, const std::string& zip_path, const Progress& progress, std::string* why);

struct Entry
{
    std::string name;   // as in the archive; a folder's ends in '/'
    uint64_t    size = 0, packed = 0, offset = 0;   // offset: its local header's
    uint32_t    crc = 0;
    uint16_t    method = 0;   // 0 stored, 8 deflated
    bool        folder = false;
};
// Whether a name in an archive may be written under a folder: relative,
// '/' between its parts, none of them empty, "." or "..", no '\' nor ':'.
bool safe_name(const std::string& name);
// The archive's entries, every one checked (its name, its method, the
// sizes and their total); false, with why, at the first that is not.
bool read_entries(const std::string& zip_path, std::vector<Entry>* out, std::string* why);
// Every entry written under `dir` (its folders made), each file checked
// against its CRC-32 and size; `after_file` after each one (a save's
// commit: its journal has room for so much at a time), false stopping it.
// The ".empty" mark is not written.
bool unzip_to(const std::string& zip_path, const std::string& dir, const std::function<bool(const std::string&)>& after_file,
              const Progress& progress, std::string* why);

// Everything under `dir` deleted, `dir` itself kept.
bool clear_dir(const std::string& dir, std::string* why);

}   // namespace save_archive
