// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/cnmt.hpp"

#include <cstring>
#include <utility>

#include "install/bytes.hpp"

namespace install
{
namespace
{
constexpr size_t HEADER = 0x20, PACKAGED_INFO = 0x38, META_INFO = 0x10, DIGEST = 0x20;
constexpr size_t INSTALL_HEADER = 8, INSTALL_INFO = 0x18;

std::optional<ContentMeta> fail(std::string* why, const char* text)
{
    if (why) *why = text;
    return std::nullopt;
}

bool has_required_system_version(MetaType t) { return t == MetaType::Application || t == MetaType::Patch; }

// Where a type's extended header says how long its extended data is.
std::optional<size_t> extended_data_size_at(MetaType t)
{
    if (t == MetaType::Patch) return 8 + 4;   // application id, required system version, then the size
    if (t == MetaType::Delta) return 8;       // application id, then the size
    return std::nullopt;
}

// NcmContentInfo (0x18): the id, the size in 5 bytes (libnx's size_low and
// size_high), the attributes, the type, the id offset. The packaged infos
// hold the same after their hash.
void put_info(uint8_t* p, const ContentInfo& c)
{
    std::memcpy(p, c.id.data(), 16);
    put_le32(p + 16, (uint32_t)c.size);
    p[20] = (uint8_t)(c.size >> 32);
    p[21] = c.attributes;
    p[22] = std::to_underlying(c.type);
    p[23] = c.id_offset;
}

ContentInfo get_info(const uint8_t* p)
{
    ContentInfo c;
    std::memcpy(c.id.data(), p, 16);
    c.size = (uint64_t)le32(p + 16) | (uint64_t)p[20] << 32;
    c.attributes = p[21];
    c.type = (ContentType)p[22];
    c.id_offset = p[23];
    return c;
}
}   // namespace

uint64_t ContentMeta::application_id() const
{
    if ((type == MetaType::Patch || type == MetaType::AddOnContent || type == MetaType::Delta) && extended_header.size() >= 8)
        return le64(extended_header.data());
    return title_id;
}

uint32_t ContentMeta::required_system_version() const
{
    if (has_required_system_version(type) && extended_header.size() >= 12) return le32(extended_header.data() + 8);
    return 0;
}

std::vector<ContentInfo> ContentMeta::installable() const
{
    std::vector<ContentInfo> out;
    for (const ContentInfo& c : contents)
        if (c.type != ContentType::DeltaFragment) out.push_back(c);
    return out;
}

std::optional<ContentMeta> cnmt_parse(std::span<const uint8_t> data, std::string* why)
{
    if (data.size() < HEADER) return fail(why, "content meta header cut short");
    ContentMeta m;
    m.title_id = le64(data.data());
    m.version = le32(data.data() + 8);
    m.type = (MetaType)data[0xc];
    const size_t ext = le16(data.data() + 0xe), contents = le16(data.data() + 0x10), metas = le16(data.data() + 0x12);
    m.attributes = data[0x14];
    m.required_download_system_version = le32(data.data() + 0x18);

    size_t at = HEADER;
    if (data.size() - at < ext) return fail(why, "extended header cut short");
    m.extended_header.assign(data.data() + at, data.data() + at + ext);
    at += ext;
    if ((data.size() - at) / PACKAGED_INFO < contents) return fail(why, "content infos cut short");
    for (size_t i = 0; i < contents; i++, at += PACKAGED_INFO) {
        const uint8_t* p = data.data() + at;
        ContentInfo    c = get_info(p + 32);
        std::memcpy(c.hash.data(), p, 32);
        m.contents.push_back(c);
    }
    if ((data.size() - at) / META_INFO < metas) return fail(why, "content meta infos cut short");
    for (size_t i = 0; i < metas; i++, at += META_INFO) {
        const uint8_t* p = data.data() + at;
        m.metas.push_back(MetaInfo{le64(p), le32(p + 8), p[12], p[13]});
    }
    if (const auto ds = extended_data_size_at(m.type)) {
        if (ext < *ds + 4) return fail(why, "extended header too short for its type");
        const size_t extended = le32(m.extended_header.data() + *ds);
        if (data.size() - at < extended) return fail(why, "extended data cut short");
        m.extended_data.assign(data.data() + at, data.data() + at + extended);
        at += extended;
    }
    if (data.size() - at < DIGEST) return fail(why, "digest cut short");
    return m;
}

std::vector<uint8_t> cnmt_install_record(const ContentMeta& meta, const ContentInfo& meta_content,
                                         bool ignore_required_system_version)
{
    const std::vector<ContentInfo> contents = meta.installable();
    std::vector<uint8_t>           out(INSTALL_HEADER + meta.extended_header.size() + (contents.size() + 1) * INSTALL_INFO +
                                       meta.metas.size() * META_INFO + meta.extended_data.size());
    uint8_t* p = out.data();
    put_le16(p, (uint16_t)meta.extended_header.size());
    put_le16(p + 2, (uint16_t)(contents.size() + 1));
    put_le16(p + 4, (uint16_t)meta.metas.size());
    p[6] = meta.attributes;
    p[7] = 0;   // storage id: ncm's own
    p += INSTALL_HEADER;
    if (!meta.extended_header.empty()) std::memcpy(p, meta.extended_header.data(), meta.extended_header.size());
    if (ignore_required_system_version && has_required_system_version(meta.type) && meta.extended_header.size() >= 12)
        put_le32(p + 8, 0);
    p += meta.extended_header.size();
    put_info(p, meta_content);
    p += INSTALL_INFO;
    for (const ContentInfo& c : contents) {
        put_info(p, c);
        p += INSTALL_INFO;
    }
    for (const MetaInfo& mi : meta.metas) {
        put_le64(p, mi.id);
        put_le32(p + 8, mi.version);
        p[12] = mi.type;
        p[13] = mi.attributes;
        p[14] = p[15] = 0;
        p += META_INFO;
    }
    if (!meta.extended_data.empty()) std::memcpy(p, meta.extended_data.data(), meta.extended_data.size());
    return out;
}

std::string content_id_hex(const ContentId& id)
{
    static const char HEX[] = "0123456789abcdef";
    std::string       s;
    for (uint8_t b : id) {
        s += HEX[b >> 4];
        s += HEX[b & 15];
    }
    return s;
}

std::optional<ContentId> content_id_from_hex(const std::string& hex)
{
    if (hex.size() != 32) return std::nullopt;
    ContentId id{};
    for (size_t i = 0; i < 32; i++) {
        const char c = hex[i];
        int        v;
        if (c >= '0' && c <= '9')
            v = c - '0';
        else if (c >= 'a' && c <= 'f')
            v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            v = c - 'A' + 10;
        else
            return std::nullopt;
        id[i / 2] = (uint8_t)(i % 2 ? id[i / 2] | v : v << 4);
    }
    return id;
}

}   // namespace install
