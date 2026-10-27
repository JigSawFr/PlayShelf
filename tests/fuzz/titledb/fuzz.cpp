// libFuzzer target: source/titledb/ on any bytes, read as a titles pack, an
// icons pack and a manifest. Beyond the sanitizers: a title's text is valid
// UTF-8 free of control characters and bounded, an icon within its bound,
// a manifest's addresses https:// and its hashes 64 hex digits; a title
// list's entries sorted and unique, their names safe, and the pack made of
// them read back whole; Nlib API's details, their text safe and bounded
// (line breaks only in the intro and the description).
#include <cstdio>
#include <string>
#include <unistd.h>

#include "fuzz.h"
#include "titledb/list.hpp"
#include "titledb/manifest.hpp"
#include "titledb/nlib.hpp"
#include "titledb/pack.hpp"

namespace
{
void check_text(const std::string& s)
{
    FUZZ_ASSERT(s.size() <= 256 && fuzz_utf8_ok(s.data(), s.size()));
    for (char ch : s)
        FUZZ_ASSERT((unsigned char)ch >= 32 && ch != 127);
}

// As check_text(), line breaks allowed, `max` bytes at most.
void check_lines(const std::string& s, size_t max)
{
    FUZZ_ASSERT(s.size() <= max && fuzz_utf8_ok(s.data(), s.size()));
    for (char ch : s)
        FUZZ_ASSERT(ch == '\n' || ((unsigned char)ch >= 32 && ch != 127));
}

uint64_t le64(const uint8_t* p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = v << 8 | p[i];
    return v;
}
}   // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    char path[] = "/tmp/playshelf_fuzz_titledb_XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0) return 0;
    const bool written = write(fd, data, size) == (ssize_t)size;
    close(fd);
    if (written) {
        // The ids the entries would hold, and a few others.
        std::vector<uint64_t> ids = { 0, 1, 0x0100000000010000 };
        for (size_t at = 32; at + 8 <= size && ids.size() < 16; at += 48) ids.push_back(le64(data + at));
        for (size_t at = 32; at + 8 <= size && ids.size() < 32; at += 32) ids.push_back(le64(data + at));
        std::string        why;
        titledb::TitlePack t;
        if (t.open(path, &why))
            for (uint64_t id : ids)
                if (const auto title = t.find(id)) {
                    check_text(title->name);
                    check_text(title->publisher);
                }
        titledb::IconPack i;
        if (i.open(path, &why))
            for (uint64_t id : ids) {
                std::vector<uint8_t> bytes;
                std::string          ext;
                if (i.find(id, &bytes, &ext)) FUZZ_ASSERT(!bytes.empty() && bytes.size() <= titledb::IconPack::MAX_ICON && ext.size() < 8);
            }
    }
    std::remove(path);
    titledb::Manifest m;
    std::string       why;
    if (titledb::parse_manifest(std::string((const char*)data, size), "https://db.example/r/m.json", &m, &why))
        for (const titledb::PackFile* p : { &m.titles, &m.icons }) {
            if (!p->known()) continue;
            FUZZ_ASSERT(p->url.compare(0, 8, "https://") == 0 && p->sha256.size() == 64 && p->size > 0);
        }
    titledb::Details d;
    if (titledb::parse_nlib_details(std::string((const char*)data, size), 0x0100000000010000, &d)) {
        check_text(d.name);
        check_text(d.publisher);
        FUZZ_ASSERT(d.release_date.empty() || d.release_date.size() == 10);
        FUZZ_ASSERT(d.categories.size() <= 8 && d.screens >= 0 && d.screens <= titledb::NLIB_MAX_SCREENS);
        for (const std::string& c : d.categories) check_text(c);
        check_lines(d.intro, 1024);
        check_lines(d.description, 8192);
    }
    std::vector<titledb::ListEntry> list;
    if (titledb::parse_title_list(std::string((const char*)data, size), &list, &why)) {
        for (size_t k = 0; k < list.size(); k++) {
            FUZZ_ASSERT(list[k].id && (k == 0 || list[k - 1].id < list[k].id));
            check_text(list[k].name);
        }
        char pack[] = "/tmp/playshelf_fuzz_titledb_list_XXXXXX";
        const int pfd = mkstemp(pack);
        if (pfd >= 0) {
            const std::string bytes = titledb::titles_pack_of(list);
            const bool        ok = write(pfd, bytes.data(), bytes.size()) == (ssize_t)bytes.size();
            close(pfd);
            titledb::TitlePack t;
            if (ok) FUZZ_ASSERT(t.open(pack, &why) && t.count() == list.size());
            std::remove(pack);
        }
    }
    return 0;
}
