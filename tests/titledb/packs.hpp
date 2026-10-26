// Packs as CyberFoil's tools write them, for the title database's suites.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#ifndef PLAYSHELF_TESTS_TITLEDB_PACKS_HPP
#define PLAYSHELF_TESTS_TITLEDB_PACKS_HPP

#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace packs
{
inline void put32(std::string* b, uint32_t v)
{
    for (int i = 0; i < 4; i++) b->push_back((char)(v >> (8 * i)));
}

inline void put64(std::string* b, uint64_t v)
{
    put32(b, (uint32_t)v);
    put32(b, (uint32_t)(v >> 32));
}

struct T
{
    uint64_t    id;
    std::string name, publisher;
    uint32_t    version;
};

// A titles pack as CyberFoil's tools write it.
inline std::string titles_pack(const std::vector<T>& titles, uint64_t strings_shift = 0)
{
    std::string strings(1, '\0'), table;
    for (const T& t : titles) {
        uint32_t flags = 0, name = 0, publisher = 0;
        if (!t.name.empty()) {
            name = (uint32_t)strings.size();
            strings += t.name + '\0';
            flags |= 1;
        }
        if (!t.publisher.empty()) {
            publisher = (uint32_t)strings.size();
            strings += t.publisher + '\0';
            flags |= 2;
        }
        flags |= 1u << 5 | 1u << 4;
        put64(&table, t.id);
        put32(&table, name);
        put32(&table, publisher);
        put32(&table, 0);
        put32(&table, 0);
        put64(&table, 1234);
        put32(&table, t.version);
        put32(&table, 0);
        put32(&table, 0);
        put32(&table, flags);
    }
    std::string out = "CFTITLE1";
    put32(&out, 1);
    put32(&out, 48);
    put32(&out, (uint32_t)titles.size());
    put32(&out, 0);
    put64(&out, 32 + table.size() + strings_shift);
    return out + table + strings;
}

inline std::string icons_pack(const std::map<uint64_t, std::string>& icons)
{
    std::string data, table;
    for (const auto& [id, bytes] : icons) {
        put64(&table, id);
        put64(&table, data.size());
        put32(&table, (uint32_t)bytes.size());
        table += std::string("WEBP\0\0\0\0", 8);
        put32(&table, 0);
        data += bytes;
    }
    std::string out = "CFICONP1";
    put32(&out, 1);
    put32(&out, 32);
    put32(&out, (uint32_t)icons.size());
    put32(&out, 0);
    put64(&out, 32 + table.size());
    return out + table + data;
}

inline void write(const std::string& path, const std::string& bytes)
{
    std::ofstream(path, std::ios::binary) << bytes;
}

}   // namespace packs

#endif
