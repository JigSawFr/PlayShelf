// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/content_stream.hpp"

#include <algorithm>
#include <cstring>
#include <zstd.h>

#include "install/aes.hpp"
#include "install/bytes.hpp"
#include "util/sha256.hpp"

namespace install
{
namespace
{
constexpr uint64_t MAGIC_SECTN = 0x4e544345535a434eull;   // "NCZSECTN"
constexpr uint64_t MAGIC_BLOCK = 0x4b434f4c425a434eull;   // "NCZBLOCK"
constexpr uint32_t MAGIC_ZSTD = 0xfd2fb528u;
constexpr uint64_t MAX_SECTIONS = 64;           // an NCA has four; nsz writes no more than that
constexpr size_t   SECTION_ENTRY = 0x40;
constexpr size_t   BLOCK_HEADER = 0x18;

struct Section
{
    uint64_t                   offset = 0, size = 0;
    uint8_t                    crypto = 0;
    std::unique_ptr<Aes128Ctr> ctr;   // type 3 only
};
}   // namespace

struct ContentStream::Impl
{
    enum class Phase { Header, Body, SectionsHead, Sections, BodyKind, Zstd, BlockHead, BlockSizes, Blocks, Raw, Done, Failed };

    std::array<uint8_t, 32>                 header_key{};
    std::optional<std::array<uint8_t, 32>> expected_hash;
    uint64_t                                expected_size = 0;
    HeaderFn                                on_header;
    WriteFn                                 write;

    Phase                phase = Phase::Header;
    std::vector<uint8_t> buf;   // what a phase gathers before it can go on
    uint64_t             nca = 0;   // the NCA's size, from its header
    uint64_t             out = 0;   // NCA bytes written
    Sha256               sha;
    bool                 ncz = false;

    std::vector<Section> sections;
    ZSTD_DCtx*           dctx = nullptr;
    std::vector<uint8_t> zout;
    // NCZBLOCK
    uint64_t              block_size = 0, block_total = 0;
    uint32_t              block_count = 0, block_index = 0;
    std::vector<uint32_t> block_sizes;
    uint64_t              block_in = 0;    // compressed bytes of the current block taken
    uint64_t              block_out = 0;   // its decompressed bytes written
    bool                  block_stored = false;

    ~Impl()
    {
        if (dctx) ZSTD_freeDCtx(dctx);
    }

    // Takes from [*p, *p + *n) until buf holds `want` bytes; true once it does.
    static bool gather(std::vector<uint8_t>& b, size_t want, const uint8_t** p, size_t* n)
    {
        const size_t take = std::min(*n, want - std::min(want, b.size()));
        b.insert(b.end(), *p, *p + take);
        *p += take;
        *n -= take;
        return b.size() >= want;
    }

    // NCA bytes out, at `out`: hashed (unless the header's own, hashed as
    // published) and written.
    bool emit(const uint8_t* data, size_t size, std::string* why, bool hashed = false)
    {
        if (!size) return true;
        if (size > nca - out) {
            *why = "more data than the NCA's size";
            return false;
        }
        if (!hashed) sha.update({data, size});
        if (!write(out, data, size)) {
            *why = "the content could not be written";
            return false;
        }
        out += size;
        return true;
    }

    // Plain NCA bytes of a .ncz at `out`: encrypted again where a CTR section lies.
    bool emit_plain(uint8_t* data, size_t size, std::string* why)
    {
        for (const Section& s : sections) {
            if (!s.ctr) continue;
            const uint64_t from = std::max(out, s.offset), to = std::min(out + size, s.offset + s.size);
            if (from < to) s.ctr->crypt({data + (from - out), (size_t)(to - from)}, from);
        }
        return emit(data, size, why);
    }

    bool header(std::string* why)
    {
        uint8_t plain[NCA_HEADER];
        std::memcpy(plain, buf.data(), NCA_HEADER);
        nca_header_crypt(plain, header_key.data(), false);
        const auto h = nca_parse_header(plain, 0, why);
        if (!h) return false;
        if (expected_size && h->size != expected_size) {
            *why = "the NCA's size differs from what the content meta gives";
            return false;
        }
        if (on_header && !on_header(*h, nca_check_signature(plain), why)) return false;
        nca = h->size;
        sha.update(buf);   // as published
        if (h->distribution == 1) nca_make_download(buf.data(), header_key.data());
        return emit(buf.data(), buf.size(), why, true);
    }

    bool parse_sections(std::string* why)
    {
        const uint64_t count = le64(buf.data() + 8);
        sections.clear();
        for (uint64_t i = 0; i < count; i++) {
            const uint8_t* e = buf.data() + 16 + i * SECTION_ENTRY;
            Section        s;
            s.offset = le64(e);
            s.size = le64(e + 8);
            s.crypto = e[16];
            uint64_t end;
            if (s.offset < NCA_FULL_HEADER || !add_ok(s.offset, s.size, &end) || end > nca) {
                *why = "an NCZ section out of the NCA";
                return false;
            }
            if (s.crypto == 3) {
                uint8_t iv[16] = {};
                std::memcpy(iv, e + 0x30, 8);   // the counter's upper half; Aes128Ctr adds offset / 16
                s.ctr = std::make_unique<Aes128Ctr>(e + 0x20, iv);
            }
            sections.push_back(std::move(s));
        }
        std::vector<const Section*> sorted;
        sorted.reserve(sections.size());
        for (const Section& s : sections)
            sorted.push_back(&s);
        std::sort(sorted.begin(), sorted.end(), [](const Section* a, const Section* b) { return a->offset < b->offset; });
        for (size_t i = 1; i < sorted.size(); i++)
            if (sorted[i]->offset < sorted[i - 1]->offset + sorted[i - 1]->size) {
                *why = "NCZ sections overlap";
                return false;
            }
        return true;
    }

    bool start_zstd(std::string* why)
    {
        if (!dctx) dctx = ZSTD_createDCtx();
        if (!dctx) {
            *why = "no memory for zstd";
            return false;
        }
        ZSTD_DCtx_reset(dctx, ZSTD_reset_session_only);
        if (zout.empty()) zout.resize(ZSTD_DStreamOutSize());
        return true;
    }

    // Decompresses [*p, *p + n) into NCA bytes; `limit` bounds what may come out.
    bool inflate(const uint8_t* p, size_t n, uint64_t limit, uint64_t* produced, size_t* frame_left, std::string* why)
    {
        ZSTD_inBuffer in = {p, n, 0};
        while (in.pos < in.size) {
            ZSTD_outBuffer o = {zout.data(), zout.size(), 0};
            const size_t   r = ZSTD_decompressStream(dctx, &o, &in);
            if (ZSTD_isError(r)) {
                *why = std::string("zstd: ") + ZSTD_getErrorName(r);
                return false;
            }
            if (o.pos > limit - *produced) {
                *why = "more decompressed data than announced";
                return false;
            }
            *produced += o.pos;
            if (!emit_plain(zout.data(), o.pos, why)) return false;
            *frame_left = r;
            if (r == 0 && in.pos < in.size) {
                // A frame ended: the rest is another one (a stream of several).
                ZSTD_DCtx_reset(dctx, ZSTD_reset_session_only);
            }
        }
        // Output zstd still holds back with no more input.
        for (;;) {
            ZSTD_outBuffer o = {zout.data(), zout.size(), 0};
            const size_t   r = ZSTD_decompressStream(dctx, &o, &in);
            if (ZSTD_isError(r)) {
                *why = std::string("zstd: ") + ZSTD_getErrorName(r);
                return false;
            }
            if (!o.pos) break;
            if (o.pos > limit - *produced) {
                *why = "more decompressed data than announced";
                return false;
            }
            *produced += o.pos;
            if (!emit_plain(zout.data(), o.pos, why)) return false;
            *frame_left = r;
        }
        return true;
    }

    size_t   zstd_left = 1;   // the stream form: 0 once its frame ended
    uint64_t zstd_produced = 0;

    bool next_block(std::string* why)
    {
        block_in = block_out = 0;
        if (block_index >= block_count) return true;
        const uint64_t expected = block_index + 1 == block_count ? block_total - (uint64_t)block_index * block_size : block_size;
        const uint32_t c = block_sizes[block_index];
        if (c == 0 || c > expected) {
            *why = "an NCZ block's compressed size out of range";
            return false;
        }
        block_stored = c == expected;
        return block_stored || start_zstd(why);
    }

    bool feed(const uint8_t* p, size_t n, std::string* why)
    {
        while (n || phase == Phase::BodyKind) {
            switch (phase) {
            case Phase::Header:
                if (!gather(buf, NCA_FULL_HEADER, &p, &n)) return true;
                if (!header(why)) return false;
                buf.clear();
                phase = Phase::Body;
                break;
            case Phase::Body:
                // A plain NCA's body, unless it starts with NCZSECTN.
                if (!gather(buf, 8, &p, &n)) return true;
                if (le64(buf.data()) == MAGIC_SECTN) {
                    ncz = true;
                    phase = Phase::SectionsHead;
                    break;
                }
                if (!emit(buf.data(), buf.size(), why)) return false;
                buf.clear();
                phase = Phase::Raw;
                break;
            case Phase::Raw: {
                const size_t take = n;
                if (!emit(p, take, why)) return false;
                p += take;
                n -= take;
                break;
            }
            case Phase::SectionsHead:
                if (!gather(buf, 16, &p, &n)) return true;
                if (le64(buf.data() + 8) > MAX_SECTIONS) {
                    *why = "too many NCZ sections";
                    return false;
                }
                phase = Phase::Sections;
                break;
            case Phase::Sections:
                if (!gather(buf, 16 + le64(buf.data() + 8) * SECTION_ENTRY, &p, &n)) return true;
                if (!parse_sections(why)) return false;
                buf.clear();
                phase = Phase::BodyKind;
                break;
            case Phase::BodyKind:
                if (!n && buf.size() < 8) return true;
                if (!gather(buf, 8, &p, &n)) return true;
                if (le64(buf.data()) == MAGIC_BLOCK) {
                    phase = Phase::BlockHead;
                } else if (le32(buf.data()) == MAGIC_ZSTD) {
                    if (!start_zstd(why)) return false;
                    phase = Phase::Zstd;
                    std::vector<uint8_t> first;
                    first.swap(buf);
                    if (!inflate(first.data(), first.size(), nca - NCA_FULL_HEADER, &zstd_produced, &zstd_left, why))
                        return false;
                } else {
                    *why = "an NCZ body neither zstd nor NCZBLOCK";
                    return false;
                }
                break;
            case Phase::Zstd: {
                const size_t take = n;
                if (!inflate(p, take, nca - NCA_FULL_HEADER, &zstd_produced, &zstd_left, why)) return false;
                p += take;
                n -= take;
                break;
            }
            case Phase::BlockHead: {
                if (!gather(buf, BLOCK_HEADER, &p, &n)) return true;
                const uint8_t* h = buf.data();
                const uint8_t  exponent = h[11];
                block_count = le32(h + 12);
                block_total = le64(h + 16);
                if (h[9] != 1 || exponent < 14 || exponent > 32) {
                    *why = "an NCZBLOCK header of an unknown kind";
                    return false;
                }
                block_size = 1ull << exponent;
                if (block_total != nca - NCA_FULL_HEADER || block_count != (block_total + block_size - 1) / block_size) {
                    *why = "NCZBLOCK sizes that do not add up to the NCA's";
                    return false;
                }
                buf.clear();
                phase = Phase::BlockSizes;
                break;
            }
            case Phase::BlockSizes:
                if (!gather(buf, (size_t)block_count * 4, &p, &n)) return true;
                block_sizes.resize(block_count);
                for (uint32_t i = 0; i < block_count; i++)
                    block_sizes[i] = le32(buf.data() + 4 * i);
                buf.clear();
                block_index = 0;
                if (!next_block(why)) return false;
                phase = Phase::Blocks;
                break;
            case Phase::Blocks: {
                if (block_index >= block_count) {
                    *why = "data after the last NCZ block";
                    return false;
                }
                const uint64_t c = block_sizes[block_index];
                const uint64_t expected =
                    block_index + 1 == block_count ? block_total - (uint64_t)block_index * block_size : block_size;
                const size_t take = (size_t)std::min<uint64_t>(n, c - block_in);
                if (block_stored) {
                    std::vector<uint8_t> copy(p, p + take);
                    if (!emit_plain(copy.data(), copy.size(), why)) return false;
                    block_out += take;
                } else {
                    size_t frame_left = 1;
                    if (!inflate(p, take, expected, &block_out, &frame_left, why)) return false;
                    if (block_in + take == c && (frame_left != 0 || block_out != expected)) {
                        *why = "an NCZ block that does not decompress to its size";
                        return false;
                    }
                }
                block_in += take;
                p += take;
                n -= take;
                if (block_in == c) {
                    block_index++;
                    if (!next_block(why)) return false;
                }
                break;
            }
            case Phase::Done:
            case Phase::Failed:
                *why = "data after the end";
                return false;
            }
        }
        return true;
    }

    bool finish(std::string* why)
    {
        if (phase == Phase::Header) {
            *why = "the NCA's header cut short";
            return false;
        }
        if (phase == Phase::Body && !buf.empty()) {
            // A plain NCA whose body is shorter than 8 bytes.
            if (!emit(buf.data(), buf.size(), why)) return false;
            buf.clear();
        }
        if (phase == Phase::SectionsHead || phase == Phase::Sections || phase == Phase::BodyKind ||
            phase == Phase::BlockHead || phase == Phase::BlockSizes) {
            *why = "the NCZ cut short";
            return false;
        }
        if (phase == Phase::Zstd && zstd_left != 0) {
            *why = "the NCZ's zstd stream cut short";
            return false;
        }
        if (phase == Phase::Blocks && block_index < block_count) {
            *why = "the NCZ's blocks cut short";
            return false;
        }
        if (out != nca) {
            *why = "the NCA came out shorter than its size";
            return false;
        }
        auto digest = sha.digest();
        if (expected_hash && digest != *expected_hash) {
            *why = "the content's SHA-256 differs from the content meta's";
            return false;
        }
        return true;
    }
};

ContentStream::ContentStream(const uint8_t header_key[32], std::optional<std::array<uint8_t, 32>> expected_hash,
                             uint64_t expected_size, HeaderFn on_header, WriteFn write)
    : impl(std::make_unique<Impl>())
{
    std::memcpy(impl->header_key.data(), header_key, 32);
    impl->expected_hash = expected_hash;
    impl->expected_size = expected_size;
    impl->on_header = std::move(on_header);
    impl->write = std::move(write);
}

ContentStream::~ContentStream() = default;

bool ContentStream::feed(std::span<const uint8_t> data)
{
    if (impl->phase == Impl::Phase::Failed) return false;
    if (impl->phase == Impl::Phase::Done) {
        err = "data after the end";
        impl->phase = Impl::Phase::Failed;
        return false;
    }
    if (!impl->feed(data.data(), data.size(), &err)) {
        impl->phase = Impl::Phase::Failed;
        return false;
    }
    return true;
}

bool ContentStream::finish()
{
    if (impl->phase == Impl::Phase::Failed) return false;
    if (!impl->finish(&err)) {
        impl->phase = Impl::Phase::Failed;
        return false;
    }
    impl->phase = Impl::Phase::Done;
    return true;
}

std::optional<ContentStream::Checkpoint> ContentStream::checkpoint() const
{
    if (impl->phase != Impl::Phase::Raw || impl->ncz) return std::nullopt;
    Checkpoint at;
    at.offset = impl->out;
    at.nca_size = impl->nca;
    at.sha = impl->sha.save();
    return at;
}

bool ContentStream::resume_at(const Checkpoint& at)
{
    if (impl->phase != Impl::Phase::Header || !impl->buf.empty()) return false;
    // Within the body: past the header and the 8 bytes that told it from a .ncz.
    if (at.offset < NCA_FULL_HEADER + 8 || at.offset > at.nca_size) return false;
    if (impl->expected_size && at.nca_size != impl->expected_size) return false;
    // The hash covers every NCA byte written (the header as published).
    uint64_t hashed = 0;
    for (size_t i = Sha256::STATE_SIZE - 8; i < Sha256::STATE_SIZE; i++) hashed = hashed << 8 | at.sha[i];
    Sha256 sha;
    if (hashed != at.offset || !sha.restore(at.sha)) return false;
    impl->sha = sha;
    impl->nca = at.nca_size;
    impl->out = at.offset;
    impl->phase = Impl::Phase::Raw;
    return true;
}

bool     ContentStream::compressed() const { return impl->ncz; }
uint64_t ContentStream::written() const { return impl->out; }
uint64_t ContentStream::nca_size() const { return impl->nca; }

}   // namespace install
