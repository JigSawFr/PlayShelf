// report_qr — a diagnostic report carried by QR codes, for a bug report that
// leaves the console without a network or an account: the report is
// compressed (zstd), cut into pieces, and each piece is one short text for a
// QR code. The user photographs the codes; tools/decode_report.py (or Reader
// here) puts the pieces back together.
//
// A piece is "<magic><schema>.<id>.<index>.<count>.<crc>:<base64>": the magic
// says whose report it is ("PS" PlayShelf, "PG" PlayGuard), `schema` is the
// format's version (1), `id` ties the pieces of one report together (8 hex),
// `index` counts from 1, `crc` is the CRC-32 of the piece's bytes (8 hex). A
// reader skips a piece of a newer schema without an error and never reads
// what it does not know.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace report_qr
{
constexpr int SCHEMA = 1;

struct Options
{
    std::string magic = "PS";          // "PS" or "PG"
    size_t      chunk_bytes = 600;     // compressed bytes in a piece (a QR code a phone reads easily)
    size_t      max_chunks = 12;       // the report is trimmed to fit this many pieces
};

struct Encoded
{
    std::vector<std::string> pieces;   // empty: even the head did not fit
    bool                     trimmed = false;   // the start of the tail was left out
};

// `head` (header, sections: kept whole) then `tail` (the end of the log: its
// start is cut first, line by line, until the pieces fit). `id` names the report.
[[nodiscard]] Encoded encode(const std::string& head, const std::string& tail, uint32_t id, const Options& opt = {});

// The pieces back, in any order, twice or from two reports.
class Reader
{
public:
    enum class Add
    {
        Ok,          // a new piece of this report
        Duplicate,   // already had it
        Ignored,     // a newer schema, or not a piece of ours at all
        Bad,         // damaged: the CRC or the layout is wrong
        OtherReport  // from another report than the first piece read: not taken
    };

    Add add(const std::string& piece);

    [[nodiscard]] size_t have() const { return pieces.size(); }
    [[nodiscard]] size_t total() const { return count; }   // 0 until a piece is read
    [[nodiscard]] bool   complete() const { return count != 0 && pieces.size() == count; }
    // The report once complete and sound; nullopt otherwise.
    [[nodiscard]] std::optional<std::string> text() const;

private:
    uint32_t                         id = 0;
    size_t                           count = 0;
    std::vector<std::optional<std::string>> pieces_by_index;
    std::vector<size_t>              pieces;   // the indexes held
};

uint32_t crc32(const std::string& data);

}   // namespace report_qr
