// nca — an NCA's header, the part of a content the installer reads: its
// 0xC00 bytes are encrypted with the console's header key (AES-XTS,
// Nintendo's tweak, 0x200-byte sectors), signed with Nintendo's fixed key
// (RSA-2048-PSS over 0x200..0x400), and say the content's size, type and
// sections. A game card's NCA says so in its header ("distribution"): the
// installer makes it a download one, as the system's own does, before
// writing it. A .cnmt.nca, small, is read whole: its first section is a
// PFS0 holding the title's content meta (cnmt.hpp), encrypted with AES-CTR
// under a key the header's key area holds, itself encrypted with a key the
// console derives (NcaKeys: core/ on the Switch, test keys on the host).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace install
{

constexpr size_t   NCA_HEADER = 0xc00;
constexpr size_t   NCA_FULL_HEADER = 0x4000;   // what an installer writes before the body (header and padding)
constexpr uint64_t NCA_MEDIA = 0x200;          // a section's start and end are counted in these

struct NcaKeys
{
    std::array<uint8_t, 32> header_key{};
    // Decrypts one 16-byte key of a header's key area: with the key area
    // key of that generation and index (0 application, 1 ocean, 2 system).
    // False when the console has no such key (a generation newer than its
    // firmware).
    std::function<bool(uint8_t generation, uint8_t index, const uint8_t in[16], uint8_t out[16])> unwrap;
};

enum class NcaEncryption : uint8_t { Auto = 0, None = 1, Xts = 2, Ctr = 3, Bktr = 4 };

struct NcaSection
{
    bool                      present = false;
    uint64_t                  start = 0, end = 0;   // bytes, from the NCA's start
    uint8_t                   fs_type = 0;          // 0 RomFS, 1 PartitionFS
    uint8_t                   hash_type = 0;
    NcaEncryption             encryption = NcaEncryption::Auto;
    uint64_t                  ctr = 0;   // the section's upper counter (CTR, BKTR)
    std::array<uint8_t, 0x200> fs_header{};
};

struct NcaHeader
{
    uint8_t                    distribution = 0;   // 0 download, 1 game card
    uint8_t                    content_type = 0;   // 0 program, 1 meta, 2 control, 3 manual, 4 data, 5 public data
    uint8_t                    key_generation = 0;   // the larger of the header's two fields
    uint8_t                    key_area_index = 0;
    uint8_t                    signature_key_generation = 0;
    uint64_t                   size = 0;
    uint64_t                   program_id = 0;
    uint32_t                   sdk_version = 0;
    std::array<uint8_t, 16>    rights_id{};
    std::array<NcaSection, 4>  sections{};
    std::array<uint8_t, 0x40>  key_area{};   // as stored: encrypted

    bool has_rights_id() const;   // its key is a title key, from a ticket
    // The key area index generation as the key derivation counts it (0 for
    // the first two, then one less than the header's).
    uint8_t key_area_generation() const { return key_generation ? (uint8_t)(key_generation - 1) : 0; }
};

// Decrypts (or encrypts) the header's 0xC00 bytes in place.
void nca_header_crypt(uint8_t header[NCA_HEADER], const uint8_t header_key[32], bool encrypt);

// A decrypted header's fields, checked: NCA3, the size, each present
// section within it and its fs header matching the hash the header keeps.
// nca_size, when known (non-zero: the file's size in its archive), must be
// the header's.
std::optional<NcaHeader> nca_parse_header(const uint8_t header[NCA_HEADER], uint64_t nca_size = 0, std::string* why = nullptr);

enum class NcaSignature { Valid, Invalid, UnknownKey };
// The fixed-key signature of a decrypted header: UnknownKey for a signature
// key generation the installer has no modulus for.
NcaSignature nca_check_signature(const uint8_t header[NCA_HEADER]);
// For the tests: the same check against another modulus.
bool nca_signature_valid_with(const uint8_t header[NCA_HEADER], const uint8_t modulus[256]);

// A game card NCA's encrypted header made a download one's (distribution 0,
// encrypted again): true when it changed anything.
bool nca_make_download(uint8_t header[NCA_HEADER], const uint8_t header_key[32]);

// The section key of a CTR section (key area slot 2), through keys.unwrap.
std::optional<std::array<uint8_t, 16>> nca_ctr_key(const NcaHeader& h, const NcaKeys& keys);
// The counter block a CTR section starts from: its upper counter, the rest
// zero (Aes128Ctr adds the offset / 16 from the NCA's start).
std::array<uint8_t, 16> nca_section_iv(const NcaSection& s);

// The content meta inside a whole .cnmt.nca: the bytes of the first file
// ending in .cnmt of its first section's PFS0.
std::optional<std::vector<uint8_t>> nca_read_cnmt(const uint8_t* nca, size_t size, const NcaKeys& keys,
                                                  std::string* why = nullptr);

}   // namespace install
