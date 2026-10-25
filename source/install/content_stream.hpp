// content_stream — one content (an NCA) as it arrives, in pieces of any
// size, turned into the bytes to write to its placeholder: from a .nca as
// it is, from a .ncz (an NCA whose sections are decrypted and compressed
// with zstd, by nsz) decompressed and encrypted again, either way with a
// game card NCA's header made a download one's.
//
// The header comes first, whole: the stream decrypts it, checks it
// (nca.hpp) and hands it to `on_header` before anything is written, which
// may refuse it (a signature the user did not accept, no room) or make the
// placeholder its size gives. Then every byte goes to `write`, at its offset
// in the NCA, in order. The bytes the content's SHA-256 covers are the NCA's
// as published (the header before its distribution changed): finish()
// compares them with the hash the content meta gives, when there is one,
// and checks the NCA came out whole, no more, no less.
//
// NCZ: after the NCA's first 0x4000 bytes, "NCZSECTN" and its sections (an
// offset, a size, a crypto type, a key and a counter each), then the rest of
// the NCA either as one zstd stream or as NCZBLOCK blocks (each one zstd
// frame, or stored when it did not shrink). A section of crypto type 3
// (CTR) is encrypted again with its key and its counter's upper half, as
// the installers it comes from do; the other types are written as they
// come.
//
// A plain .nca can be stopped and taken up again later (an install paused,
// or cut by a restart): checkpoint() says where its body stands (the NCA
// bytes written, the SHA-256 so far), resume_at() starts a new stream
// there, the file then fed from that offset on (a .nca's bytes are the
// NCA's, at the same offsets). A .ncz starts again from its beginning.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "install/nca.hpp"
#include "util/sha256.hpp"

namespace install
{

class ContentStream
{
  public:
    // Gets the decrypted header's fields and its fixed-key signature's
    // check; false refuses the content (the stream stops with that reason).
    using HeaderFn = std::function<bool(const NcaHeader&, NcaSignature, std::string* why)>;
    // Writes `size` bytes at `offset` of the NCA; false stops the stream.
    using WriteFn = std::function<bool(uint64_t offset, const uint8_t* data, size_t size)>;

    // header_key: the console's. expected_hash: the content meta's SHA-256
    // of this content, or nothing to check none. expected_size: the
    // content's size as the content meta gives it (0: unknown).
    ContentStream(const uint8_t header_key[32], std::optional<std::array<uint8_t, 32>> expected_hash,
                  uint64_t expected_size, HeaderFn on_header, WriteFn write);
    ~ContentStream();
    ContentStream(const ContentStream&) = delete;
    ContentStream& operator=(const ContentStream&) = delete;

    // Where a plain NCA's body stands; nothing before its body or for a .ncz.
    struct Checkpoint
    {
        uint64_t                                offset = 0;   // the NCA bytes written, and the file's to feed next
        uint64_t                                nca_size = 0;
        std::array<uint8_t, Sha256::STATE_SIZE> sha{};
    };
    std::optional<Checkpoint> checkpoint() const;
    // Before any feed(): goes on from `at` (the header already checked and
    // written by the stream that gave it). False, the stream unchanged,
    // when `at` cannot be this content's (past its end, a size other than
    // the content meta's, a hash state that is not one).
    bool resume_at(const Checkpoint& at);

    // The next bytes of the file (.nca or .ncz). False once the stream has
    // stopped: error() says why.
    bool feed(const uint8_t* data, size_t size);
    // The file's end: true when the NCA came out whole and matches its hash.
    bool finish();

    const std::string& error() const { return err; }
    bool               compressed() const;   // a .ncz, once its body began
    uint64_t           written() const;      // NCA bytes written so far
    uint64_t           nca_size() const;     // from the header, once it came

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::string           err;
};

}   // namespace install
