// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/report_qr.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <zstd.h>

namespace report_qr
{
namespace
{
constexpr size_t MAX_REPORT = 4u << 20;   // what a report may unpack to
constexpr size_t MAX_COUNT = 64;          // pieces a reader accepts

const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64_encode(const std::string& in)
{
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    for (size_t i = 0; i < in.size(); i += 3) {
        const uint32_t a = (uint8_t)in[i], b = i + 1 < in.size() ? (uint8_t)in[i + 1] : 0,
                       c = i + 2 < in.size() ? (uint8_t)in[i + 2] : 0;
        const uint32_t v = a << 16 | b << 8 | c;
        out += B64[v >> 18 & 63];
        out += B64[v >> 12 & 63];
        out += i + 1 < in.size() ? B64[v >> 6 & 63] : '=';
        out += i + 2 < in.size() ? B64[v & 63] : '=';
    }
    return out;
}

bool b64_decode(const std::string& in, std::string* out)
{
    if (in.size() % 4 != 0) return false;
    out->clear();
    for (size_t i = 0; i < in.size(); i += 4) {
        uint32_t v = 0;
        int      pad = 0;
        for (size_t k = 0; k < 4; k++) {
            const char c = in[i + k];
            const char* p = c == '=' ? nullptr : (c ? std::find(B64, B64 + 64, c) : nullptr);
            if (c == '=') {
                if (i + 4 != in.size() || k < 2) return false;
                pad++;
                v <<= 6;
            } else {
                if (pad || !p || p == B64 + 64) return false;
                v = v << 6 | (uint32_t)(p - B64);
            }
        }
        out->push_back((char)(v >> 16 & 0xff));
        if (pad < 2) out->push_back((char)(v >> 8 & 0xff));
        if (pad < 1) out->push_back((char)(v & 0xff));
    }
    return true;
}

std::string hex8(uint32_t v)
{
    char buf[16];
    std::snprintf(buf, sizeof buf, "%08x", v);
    return buf;
}

bool parse_hex8(const std::string& s, uint32_t* out)
{
    if (s.size() != 8) return false;
    uint32_t v = 0;
    for (char c : s) {
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else return false;
        v = v << 4 | (uint32_t)d;
    }
    *out = v;
    return true;
}

bool parse_num(const std::string& s, size_t* out)
{
    if (s.empty() || s.size() > 4) return false;
    size_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (size_t)(c - '0');
    }
    *out = v;
    return true;
}

std::string compress(const std::string& data)
{
    std::string out(ZSTD_compressBound(data.size()), '\0');
    ZSTD_CCtx* ctx = ZSTD_createCCtx();
    if (!ctx) return {};
    ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, 19);
    ZSTD_CCtx_setParameter(ctx, ZSTD_c_checksumFlag, 1);   // the whole report is checked once unpacked
    const size_t n = ZSTD_compress2(ctx, out.data(), out.size(), data.data(), data.size());
    ZSTD_freeCCtx(ctx);
    if (ZSTD_isError(n)) return {};
    out.resize(n);
    return out;
}
}   // namespace

uint32_t crc32(const std::string& data)
{
    uint32_t c = 0xffffffffu;
    for (unsigned char b : data) {
        c ^= b;
        for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}

Encoded encode(const std::string& head, const std::string& tail, uint32_t id, const Options& opt)
{
    Encoded     out;
    const size_t chunk = std::max<size_t>(opt.chunk_bytes, 1);
    std::string kept = tail;
    for (;;) {
        const std::string packed = compress(head + (out.trimmed ? "[...]\n" : "") + kept);
        if (packed.empty()) return out;
        const size_t n = (packed.size() + chunk - 1) / chunk;
        if (n <= opt.max_chunks && n <= MAX_COUNT) {
            for (size_t i = 0; i < n; i++) {
                const std::string raw = packed.substr(i * chunk, chunk);
                out.pieces.push_back(opt.magic + std::to_string(SCHEMA) + "." + hex8(id) + "." + std::to_string(i + 1) + "." +
                                     std::to_string(n) + "." + hex8(crc32(raw)) + ":" + b64_encode(raw));
            }
            return out;
        }
        if (kept.empty()) return out;   // the head alone is too big
        // Drop the first quarter of the tail, up to a line end.
        size_t cut = kept.size() / 4;
        const size_t nl = kept.find('\n', cut);
        cut = nl == std::string::npos ? kept.size() : nl + 1;
        kept.erase(0, cut);
        out.trimmed = true;
    }
}

Reader::Add Reader::add(const std::string& piece)
{
    // <magic 2><schema>.<id>.<index>.<count>.<crc>:<base64>
    const size_t colon = piece.find(':');
    if (colon == std::string::npos || piece.size() < 4) return Add::Ignored;
    const std::string magic = piece.substr(0, 2);
    if (magic != "PS" && magic != "PG") return Add::Ignored;
    std::vector<std::string> f;
    {
        size_t from = 2;
        while (true) {
            const size_t dot = piece.find('.', from);
            if (dot == std::string::npos || dot > colon) {
                f.push_back(piece.substr(from, colon - from));
                break;
            }
            f.push_back(piece.substr(from, dot - from));
            from = dot + 1;
        }
    }
    size_t schema = 0;
    if (f.empty() || !parse_num(f[0], &schema)) return Add::Ignored;
    if (schema > (size_t)SCHEMA) return Add::Ignored;   // a newer format: not ours to read
    if (f.size() != 5) return Add::Bad;
    uint32_t pid = 0, crc = 0;
    size_t   index = 0, total = 0;
    if (!parse_hex8(f[1], &pid) || !parse_num(f[2], &index) || !parse_num(f[3], &total) || !parse_hex8(f[4], &crc))
        return Add::Bad;
    if (total == 0 || total > MAX_COUNT || index == 0 || index > total) return Add::Bad;
    std::string raw;
    if (!b64_decode(piece.substr(colon + 1), &raw) || crc32(raw) != crc) return Add::Bad;

    if (count == 0) {
        id = pid;
        count = total;
        pieces_by_index.assign(total, std::nullopt);
    } else if (pid != id || total != count) {
        return Add::OtherReport;
    }
    if (pieces_by_index[index - 1]) return Add::Duplicate;
    pieces_by_index[index - 1] = std::move(raw);
    pieces.push_back(index);
    return Add::Ok;
}

std::optional<std::string> Reader::text() const
{
    if (!complete()) return std::nullopt;
    std::string packed;
    for (const auto& p : pieces_by_index) packed += *p;
    const unsigned long long size = ZSTD_getFrameContentSize(packed.data(), packed.size());
    if (size == ZSTD_CONTENTSIZE_ERROR || size == ZSTD_CONTENTSIZE_UNKNOWN || size > MAX_REPORT) return std::nullopt;
    std::string out((size_t)size, '\0');
    const size_t n = ZSTD_decompress(out.data(), out.size(), packed.data(), packed.size());
    if (ZSTD_isError(n) || n != out.size()) return std::nullopt;
    return out;
}

}   // namespace report_qr
