// recompress — a Program NCA with its RomFS stored through the firmware's
// compression layer (compressed_storage.hpp), made from the NCA as the
// console has it. The other sections are carried over as they are (read,
// decrypted, encrypted again where they now stand); the RomFS section is
// rebuilt: the image cut in blocks, the blocks and their table as the
// section's data, the integrity tree over that (ivfc.hpp), the file system
// header's compression info; then the header, with the sizes, the places and
// the hashes of the file system headers it keeps.
//
// What it cannot make is Nintendo's signature: the header is signed with a
// key only Nintendo has, so the NCA it makes is not signed and runs only
// where the system's signature checks are patched (the user was told so
// before this ran). The NCA's second signature and the program's own are
// left as they were.
//
// Only what is understood is rebuilt: a Program NCA with no rights id (a
// title key is not handled), one RomFS section in CTR or not encrypted, the
// other sections in CTR or not encrypted, nothing sparse, nothing already
// compressed. Anything else is refused by inspect(), before a byte is made.
//
// The body (everything after the first NCA_FULL_HEADER bytes) goes to a sink
// as it is made, since the header that comes first says how it came out; it
// is available when build() has finished.
//
// Plain C++; the codec is the caller's.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "install/compressed_storage.hpp"
#include "install/nca.hpp"

namespace install
{

// Reads the original NCA, as the console stores it (encrypted).
using ReadAtFn = std::function<bool(uint64_t offset, std::span<uint8_t> out)>;

class ProgramRecompressor
{
  public:
    ProgramRecompressor(ReadAtFn original, uint64_t original_size, NcaKeys keys, cstore::CompressFn compress,
                        cstore::DecompressFn decompress, uint32_t block_size = cstore::MAX_BLOCK);

    // Reads the header and checks the NCA can be made over; why not, with
    // nothing made.
    [[nodiscard]] Done inspect();
    const NcaHeader&   header() const { return orig; }

    // Makes the body. `progress` is given the bytes of the original read
    // and its total. Why not when it failed, was cancelled or a read or the
    // sink failed; whatever the sink was given is then worthless.
    [[nodiscard]] Done build(const cstore::SinkFn& body, const std::function<void(uint64_t done, uint64_t total)>& progress = {},
                             const std::atomic<bool>* cancel = nullptr);

    // After a successful build(): the NCA's first NCA_FULL_HEADER bytes, and
    // its whole size.
    const std::vector<uint8_t>& new_header() const { return head; }
    uint64_t                    new_size() const { return size; }
    // Checks an NCA made by build() against the original, reading both and
    // trusting nothing of the first: its header parses (sizes and hashes of
    // the file system headers), the other sections are the original's bytes
    // decrypted, the integrity tree verifies, the compression table opens,
    // and the RomFS it decompresses to is the original's, byte for byte.
    // `made` reads the new NCA; why not at the first difference.
    [[nodiscard]] Done verify(const ReadAtFn& made, uint64_t made_size,
                              const std::function<void(uint64_t done, uint64_t total)>& progress = {},
                              const std::atomic<bool>* cancel = nullptr);

    // The compression it came to.
    uint64_t                    romfs_before() const { return romfs_in; }
    uint64_t                    romfs_after() const { return romfs_out; }

  private:
    struct Stream;
    [[nodiscard]] Done copy_section(int i, Stream& out);
    [[nodiscard]] Done rebuild_romfs(int i, Stream& out, std::array<uint8_t, 0x200>& fs_header);
    bool               read_plain(int i, uint64_t from, std::span<uint8_t> out);

    ReadAtFn                orig_read;
    uint64_t                orig_size;
    NcaKeys                 keys;
    cstore::CompressFn      compress;
    cstore::DecompressFn    decompress;
    uint32_t                block_size;

    NcaHeader               orig;
    std::array<uint8_t, NCA_HEADER> plain_header{};
    std::array<uint8_t, 16> key{};
    int                     romfs = -1;
    uint64_t                first_section = ~0ull;
    uint64_t                read_done = 0, read_total = 0;
    std::function<void(uint64_t, uint64_t)> progress_fn;
    const std::atomic<bool>* cancel_flag = nullptr;

    std::vector<uint8_t>    head;
    uint64_t                size = 0, romfs_in = 0, romfs_out = 0;
    bool                    inspected = false;
};

}   // namespace install
