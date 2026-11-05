// Writes the NCAs the recompressor makes from the test builders' programs (a
// system-layout one, a packed one, an update merged in) to a folder, with the
// keys file they open with and the SHA-256 of the RomFS image each must give
// back, for tests/oracle/run.sh to check them with LibHac, a reader that shares
// no code with this one.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "../install/patch_builders.hpp"
#include "install/recompress.hpp"
#include "util/sha256.hpp"

using namespace install;
using namespace test_install;

namespace
{
std::string hex(std::span<const uint8_t> b)
{
    static const char* d = "0123456789abcdef";
    std::string        s;
    for (uint8_t x : b) {
        s += d[x >> 4];
        s += d[x & 15];
    }
    return s;
}

void put(const std::string& path, const void* data, size_t n)
{
    std::ofstream(path, std::ios::binary).write((const char*)data, (std::streamsize)n);
}

// The recompressor's output for `nca` (with `update` merged when given).
Bytes made(const Bytes& nca, const Bytes* update)
{
    auto rd = [](const Bytes& b) {
        return [&b](uint64_t off, std::span<uint8_t> out) {
            if (off > b.size() || out.size() > b.size() - off) return false;
            std::memcpy(out.data(), b.data() + off, out.size());
            return true;
        };
    };
    ProgramRecompressor rc(rd(nca), nca.size(), test_keys(), lz4_compress, lz4_decompress, 16384);
    if (update) rc.with_update(rd(*update), update->size());
    if (const Done d = rc.inspect(); !d) {
        std::fprintf(stderr, "inspect: %s\n", d.error().c_str());
        std::exit(1);
    }
    Bytes body;
    if (const Done d = rc.build([&](std::span<const uint8_t> b) { body.insert(body.end(), b.begin(), b.end()); return true; }); !d) {
        std::fprintf(stderr, "build: %s\n", d.error().c_str());
        std::exit(1);
    }
    Bytes out = rc.new_header();
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

void emit(const std::string& dir, const std::string& name, const Bytes& nca, const Bytes& image)
{
    put(dir + "/" + name + ".nca", nca.data(), nca.size());
    Sha256 h;
    h.update(image);
    const auto d = h.digest();
    const std::string text = "size=" + std::to_string(image.size()) + "\nsha256=" + hex(d) + "\n";
    put(dir + "/" + name + ".expected", text.data(), text.size());
}
}   // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: oracle_gen <folder>\n");
        return 2;
    }
    const std::string dir = argv[1];
    std::filesystem::create_directories(dir);

    std::string keys = "header_key = " + hex({HEADER_KEY, 32}) + "\n";
    for (int i = 0; i < 32; i++) {
        char k[64];
        std::snprintf(k, sizeof k, "key_area_key_application_%02x = ", i);
        keys += k + hex({KAEK, 16}) + "\n";
    }
    put(dir + "/prod.keys", keys.data(), keys.size());

    const Built system = program_nca(900'000, 11);
    emit(dir, "system", made(system.nca, nullptr), system.image);
    const Built packed = program_nca(400'000, 12, false, 0, true);
    emit(dir, "packed", made(packed.nca, nullptr), packed.image);
    const Built     base = program_nca(900'000, 13);
    const UpdateNca upd = update_nca(base, 5);
    emit(dir, "merged", made(base.nca, &upd.nca), upd.patch.patched_image);
    return 0;
}
