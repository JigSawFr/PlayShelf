// cnmt — a title's content meta: what an NSP's .cnmt.nca holds (the
// "packaged" form: the title, its version and type, and the contents to
// install with their sizes and hashes), and the record ncm keeps for it once
// installed (its NcmContentMetaHeader form), built from it.
//
// Unlike the installers it comes from, the record keeps the content meta
// infos and a patch's extended data as the packaged one has them, rather
// than a count without them and zeros. Delta fragments (an update's
// differences from an older one) are left out, as the system's own
// installer does.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace install
{

enum class MetaType : uint8_t {
    SystemProgram = 0x01,
    Application = 0x80,
    Patch = 0x81,
    AddOnContent = 0x82,
    Delta = 0x83,
    DataPatch = 0x84,
};

enum class ContentType : uint8_t {
    Meta = 0,
    Program = 1,
    Data = 2,
    Control = 3,
    HtmlDocument = 4,
    LegalInformation = 5,
    DeltaFragment = 6,
};

using ContentId = std::array<uint8_t, 16>;

struct ContentInfo
{
    std::array<uint8_t, 32> hash{};   // SHA-256 of the whole content
    ContentId               id{};
    uint64_t                size = 0;   // 40 bits
    uint8_t                 attributes = 0;
    ContentType             type = ContentType::Meta;
    uint8_t                 id_offset = 0;
};

struct MetaInfo
{
    uint64_t id = 0;
    uint32_t version = 0;
    uint8_t  type = 0;
    uint8_t  attributes = 0;
};

struct ContentMeta
{
    uint64_t                 title_id = 0;
    uint32_t                 version = 0;
    MetaType                 type = MetaType::Application;
    uint8_t                  attributes = 0;
    uint32_t                 required_download_system_version = 0;
    std::vector<uint8_t>     extended_header;
    std::vector<ContentInfo> contents;   // every one, delta fragments included
    std::vector<MetaInfo>    metas;
    std::vector<uint8_t>     extended_data;   // a patch's or delta's, after the infos

    // The application this belongs to: its own id for an application, the
    // one its extended header names for a patch or an add-on.
    uint64_t application_id() const;
    // The system version it requires (an application's or patch's extended
    // header), 0 when its type says none.
    uint32_t required_system_version() const;
    // The contents to install: every one but delta fragments.
    std::vector<ContentInfo> installable() const;
};

[[nodiscard]] std::optional<ContentMeta> cnmt_parse(const uint8_t* data, size_t size, std::string* why = nullptr);

// The record ncm keeps: NcmContentMetaHeader, the extended header, the
// content infos (the .cnmt.nca's own first, as `meta_content`, then the
// installable ones), the meta infos, the extended data.
// ignore_required_system_version zeroes an application's or patch's
// required version, for a console on an older firmware (the installer's
// option, off by default).
std::vector<uint8_t> cnmt_install_record(const ContentMeta& meta, const ContentInfo& meta_content,
                                         bool ignore_required_system_version = false);

// "0123…ef", 32 lower-case hex digits: an NCA's file name without .nca.
std::string content_id_hex(const ContentId& id);
std::optional<ContentId> content_id_from_hex(const std::string& hex);

}   // namespace install
