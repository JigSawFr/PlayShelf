// recompress_meta — what a title's content meta says once its program NCA is
// the recompressed one (recompress.hpp): the content meta itself (cnmt) with
// the program's id, size and hash replaced and the system version it needs
// raised to the firmware that reads the compression layer, and the
// .cnmt.nca that holds it, changed in place (the content meta keeps its
// length, so the NCA keeps its layout: the partition's hash table is made
// again, the file system header and the NCA header follow).
//
// Content ids follow the convention of the tools that pack homebrew: the
// first 16 bytes of the content's SHA-256.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <vector>

#include "install/cnmt.hpp"
#include "install/nca.hpp"

namespace install
{

// 12.0.0, the firmware that reads the compression layer, as a content meta
// writes a system version (major << 26 | minor << 20 | micro << 16).
constexpr uint32_t SYSTEM_VERSION_12_0_0 = 12u << 26;

// The content id the convention gives a content: its hash's first 16 bytes.
ContentId content_id_of(const std::array<uint8_t, 32>& sha256);

// `cnmt` (a packaged content meta) with the content `old_id` (a program)
// described as `now`, and, for an application or patch, a required system
// version below 12.0.0 raised to it. Same length. Why not when `old_id` is
// not a program content of it, or it is not a content meta that can be
// patched in place.
[[nodiscard]] Parsed<std::vector<uint8_t>> cnmt_with_program(std::span<const uint8_t> cnmt, const ContentId& old_id,
                                                              const ContentInfo& now);

// A whole .cnmt.nca with its content meta replaced by one of the same length:
// the same NCA (same size, same sections) with the partition's hash table, the
// file system header's master hash and the NCA header's hash of it made
// again. Its signature is left as it was (it no longer matches).
[[nodiscard]] Parsed<std::vector<uint8_t>> meta_nca_with_cnmt(std::span<const uint8_t> nca, std::span<const uint8_t> cnmt,
                                                               const NcaKeys& keys);

// The contents an installed record (what ncm keeps) lists, the .cnmt.nca's own
// first; the record has no hashes. Why not when it is cut short.
[[nodiscard]] Parsed<std::vector<ContentInfo>> record_contents(std::span<const uint8_t> record);

// The record with some contents described otherwise (their id and size; the
// attributes and type stay), and, for an application or patch, a required
// system version below 12.0.0 raised to it. Same length. Why not when a
// content to replace is not in it, or twice.
struct ContentReplacement
{
    ContentId old_id;
    ContentId new_id;
    uint64_t  new_size = 0;
};
[[nodiscard]] Parsed<std::vector<uint8_t>> record_with_contents(std::span<const uint8_t> record, MetaType type,
                                                                 const std::vector<ContentReplacement>& replacements);

}   // namespace install
