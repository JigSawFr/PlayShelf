// saves/archive.cpp: a save's folder made a zip (deflated, its sizes after
// each entry) and read back the same; an empty save as its ".empty" mark;
// a stored archive (another tool's) read; names that would leave the
// folder, a damaged archive and an encrypted one refused; a folder
// emptied.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <ctime>
#include <string>

#include "check.h"
#include "fs.hpp"
#include "saves/archive.hpp"
#include "temp_dir.hpp"
#include "util/zip_write.hpp"

using namespace save_archive;

namespace
{
std::string big()
{
    std::string s;
    for (int i = 0; i < 300000; i++) s += (char)('a' + (int)(((unsigned)i * 7919u) % 26u));
    return s;
}

void test_round_trip()
{
    TempDir t("save_archive");
    const std::string src = t.path() + "/save", dst = t.path() + "/back", zip = t.path() + "/s.zip";
    fs::make_dirs(src + "/slot1/deep");
    fs::make_dirs(src + "/empty");
    fs::write(src + "/system.dat", std::string("\0\1\2", 3));
    fs::write(src + "/slot1/progress.bin", big());
    fs::write(src + "/slot1/deep/zero.bin", "");
    size_t calls = 0;
    std::string why;
    if (!CHECK(zip_dir(src, zip, [&](size_t, size_t total, const std::string&) { calls++; CHECK(total == 3); }, &why))) {
        fprintf(stderr, "  %s\n", why.c_str());
        return;
    }
    CHECK(calls == 4);   // each file, then the end
    std::vector<Entry> entries;
    CHECK(read_entries(zip, &entries, &why) && entries.size() == 6);
    fs::make_dirs(dst);
    std::vector<std::string> committed;
    CHECK(unzip_to(zip, dst, [&](const std::string& n) { committed.push_back(n); return true; }, nullptr, &why));
    CHECK(committed.size() == 3);
    CHECK(fs::read(dst + "/system.dat") == std::string("\0\1\2", 3) && fs::read(dst + "/slot1/progress.bin") == big());
    CHECK(fs::exists(dst + "/slot1/deep/zero.bin") && fs::read(dst + "/slot1/deep/zero.bin").empty());
    CHECK(fs::exists(dst + "/empty"));
    // Stopped by its commit.
    CHECK(!unzip_to(zip, dst, [](const std::string&) { return false; }, nullptr, &why) && why == "stopped");
    // Emptied, the folder itself kept.
    CHECK(clear_dir(dst, &why) && fs::exists(dst));
    std::vector<Entry> none;
    CHECK(!fs::exists(dst + "/slot1") && !fs::exists(dst + "/system.dat"));
    // An empty save: its mark, read back as nothing.
    const std::string empty = t.path() + "/nothing", ezip = t.path() + "/e.zip";
    fs::make_dirs(empty);
    CHECK(zip_dir(empty, ezip, nullptr, &why) && read_entries(ezip, &none, &why) && none.size() == 1 && none[0].name == EMPTY_MARK);
    CHECK(unzip_to(ezip, dst, nullptr, nullptr, &why) && !fs::exists(dst + "/" + EMPTY_MARK));
}

void test_other_archives()
{
    TempDir t("save_archive_other");
    std::tm now{};
    now.tm_year = 126;
    now.tm_mday = 1;
    const std::string stored = t.path() + "/stored.zip", out = t.path() + "/out";
    fs::write(stored, zip_write::stored(now, {{"a.bin", "hello"}, {"dir/b.bin", "world"}}));
    fs::make_dirs(out);
    std::string why;
    CHECK(unzip_to(stored, out, nullptr, nullptr, &why) && fs::read(out + "/dir/b.bin") == "world");
    // Names that would leave the folder.
    for (const char* bad : {"../x", "/abs", "a/../../x", "c:/x", "a\\b", "a//b", "./a", ""}) {
        const std::string evil = t.path() + "/evil.zip";
        fs::write(evil, zip_write::stored(now, {{bad, "x"}}));
        std::vector<Entry> e;
        CHECK(!read_entries(evil, &e, &why));
    }
    CHECK(safe_name("a/b/c.bin") && safe_name("folder/") && !safe_name("a/./b") && !safe_name("..") && !safe_name("/"));
    // Damaged: cut short, its CRC wrong, no zip at all, encrypted.
    std::string z = zip_write::stored(now, {{"a.bin", "hello world"}});
    const std::string cut = t.path() + "/cut.zip";
    fs::write(cut, z.substr(0, z.size() - 10));
    std::vector<Entry> e;
    CHECK(!read_entries(cut, &e, &why));
    std::string flipped = z;
    flipped[30 + 5] ^= 1;   // a byte of its data
    fs::write(cut, flipped);
    CHECK(!unzip_to(cut, out, nullptr, nullptr, &why) && why.find("damaged") != std::string::npos);
    fs::write(cut, "not a zip at all, but longer than an end record");
    CHECK(!read_entries(cut, &e, &why));
    std::string locked = z;
    const size_t cd = locked.find("PK\x01\x02");
    locked[cd + 8] |= 1;
    fs::write(cut, locked);
    CHECK(!read_entries(cut, &e, &why) && why.find("encrypted") != std::string::npos);
}
}   // namespace

int main()
{
    test_round_trip();
    test_other_archives();
    return CHECK_DONE("save archive: round trip, empty save, other archives, unsafe names, damage passed");
}
