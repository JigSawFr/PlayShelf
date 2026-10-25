// Inputs the install suites build: a fixed pseudo-random stream, and PFS0
// or HFS0 archives of given files.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "install/bytes.hpp"
#include "install/pfs.hpp"
#include "util/sha256.hpp"

namespace test_install
{
using Bytes = std::vector<uint8_t>;
using namespace install;

// make_vectors.py's stream(): SHA-256 of the seed and a big-endian counter.
inline Bytes stream(size_t n, const char* seed)
{
    Bytes out;
    for (uint32_t i = 0; out.size() < n; i++) {
        Sha256        h;
        const uint8_t c[4] = {(uint8_t)(i >> 24), (uint8_t)(i >> 16), (uint8_t)(i >> 8), (uint8_t)i};
        h.update(seed, std::strlen(seed));
        h.update(c, 4);
        const auto d = h.digest();
        out.insert(out.end(), d.begin(), d.end());
    }
    out.resize(n);
    return out;
}

struct File
{
    std::string name;
    Bytes       data;
};

// A PFS0 or HFS0 archive of `files`, the string table padded to `pad`.
inline Bytes archive(PfsKind kind, const std::vector<File>& files, size_t pad = 0x20)
{
    const size_t esize = kind == PfsKind::Pfs0 ? 0x18 : 0x40;
    std::string  names;
    for (const File& f : files)
        names += f.name + '\0';
    while (names.size() % pad)
        names += '\0';
    Bytes out(PFS_BASE_HEADER + files.size() * esize + names.size());
    put_le32(out.data(), kind == PfsKind::Pfs0 ? 0x30534650 : 0x30534648);
    put_le32(out.data() + 4, (uint32_t)files.size());
    put_le32(out.data() + 8, (uint32_t)names.size());
    uint64_t data_at = 0;
    uint32_t name_at = 0;
    for (size_t i = 0; i < files.size(); i++) {
        uint8_t* e = out.data() + PFS_BASE_HEADER + i * esize;
        put_le64(e, data_at);
        put_le64(e + 8, files[i].data.size());
        put_le32(e + 16, name_at);
        if (kind == PfsKind::Hfs0) {
            const uint32_t hashed = (uint32_t)std::min<size_t>(files[i].data.size(), 0x200);
            put_le32(e + 20, hashed);
            Sha256 h;
            h.update(files[i].data.data(), hashed);
            const auto d = h.digest();
            std::memcpy(e + 32, d.data(), 32);
        }
        data_at += files[i].data.size();
        name_at += (uint32_t)files[i].name.size() + 1;
    }
    std::memcpy(out.data() + PFS_BASE_HEADER + files.size() * esize, names.data(), names.size());
    for (const File& f : files)
        out.insert(out.end(), f.data.begin(), f.data.end());
    return out;
}

}   // namespace test_install
