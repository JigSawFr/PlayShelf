// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "titledb/pack.hpp"

#include <algorithm>
#include <cstring>

namespace titledb
{
namespace
{
constexpr char     TITLE_MAGIC[8] = { 'C', 'F', 'T', 'I', 'T', 'L', 'E', '1' };
constexpr char     ICON_MAGIC[8] = { 'C', 'F', 'I', 'C', 'O', 'N', 'P', '1' };
constexpr uint32_t HEADER = 32, TITLE_ENTRY = 48, ICON_ENTRY = 32;
constexpr uint64_t MAX_INDEX = 64u << 20;   // entries' bytes at most: a million titles and more
constexpr uint32_t HAS_NAME = 1u << 0, HAS_PUBLISHER = 1u << 1, HAS_SIZE = 1u << 4, HAS_VERSION = 1u << 5,
                   HAS_DATE = 1u << 6;
constexpr size_t   MAX_STRING = 1024;   // read of a name at most, before it is bounded

uint32_t le32(const uint8_t* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

uint64_t le64(const uint8_t* p)
{
    return (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32;
}

uint64_t size_of(FILE* f)
{
    if (fseeko(f, 0, SEEK_END) != 0) return 0;
    const off_t end = ftello(f);
    return end > 0 ? (uint64_t)end : 0;
}

bool read_at(FILE* f, uint64_t at, void* out, size_t n)
{
    return fseeko(f, (off_t)at, SEEK_SET) == 0 && std::fread(out, 1, n, f) == n;
}

// The header's fields: magic, version, entry size, count, flags, offset.
bool header(FILE* f, const char* magic, uint32_t entry_size, uint32_t* count, uint64_t* offset, std::string* why)
{
    uint8_t h[HEADER];
    if (!read_at(f, 0, h, sizeof h)) {
        if (why) *why = "too short";
        return false;
    }
    if (std::memcmp(h, magic, 8) != 0) {
        if (why) *why = "not a pack of this kind";
        return false;
    }
    if (le32(h + 8) != 1 || le32(h + 12) != entry_size) {
        if (why) *why = "a pack version not known";
        return false;
    }
    *count = le32(h + 16);
    *offset = le64(h + 24);
    return true;
}
}   // namespace

std::string safe_text(const std::string& text, size_t max)
{
    std::string out;
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = (unsigned char)text[i];
        const size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (!n || i + n > text.size()) return "";
        uint32_t cp = n == 1 ? c : n == 2 ? c & 0x1f : n == 3 ? c & 0x0f : c & 0x07;
        for (size_t k = 1; k < n; k++) {
            if (((unsigned char)text[i + k] >> 6) != 2) return "";
            cp = cp << 6 | ((unsigned char)text[i + k] & 0x3f);
        }
        // No overlong form, no surrogate, nothing past U+10FFFF.
        static const uint32_t LEAST[5] = { 0, 0, 0x80, 0x800, 0x10000 };
        if (cp < LEAST[n] || (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff) return "";
        if (out.size() + n > max) break;
        if (n == 1) out += c < 0x20 || c == 0x7f ? ' ' : (char)c;
        else out.append(text, i, n);
        i += n;
    }
    return out;
}

TitlePack::~TitlePack()
{
    if (this->file) std::fclose(this->file);
}

bool TitlePack::open(const std::string& path, std::string* why)
{
    this->index.clear();
    if (this->file) std::fclose(this->file);
    this->file = std::fopen(path.c_str(), "rb");
    if (!this->file) {
        if (why) *why = "cannot be opened";
        return false;
    }
    uint32_t count = 0;
    if (!header(this->file, TITLE_MAGIC, TITLE_ENTRY, &count, &this->strings, why)) return false;
    this->file_size = size_of(this->file);
    const uint64_t table = (uint64_t)count * TITLE_ENTRY;
    if (table > MAX_INDEX || this->strings < HEADER + table || this->strings > this->file_size) {
        if (why) *why = "an index that does not hold together";
        return false;
    }
    std::vector<uint8_t> raw((size_t)table);
    if (!read_at(this->file, HEADER, raw.data(), raw.size())) {
        if (why) *why = "too short";
        return false;
    }
    this->index.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* p = raw.data() + (size_t)i * TITLE_ENTRY;
        Entry          e{};
        e.id = le64(p);
        e.name = le32(p + 8);
        e.publisher = le32(p + 12);
        e.size = le64(p + 24);
        e.version = le32(p + 32);
        e.release_date = le32(p + 36);
        e.flags = le32(p + 44);
        if (e.id) this->index.push_back(e);
    }
    std::stable_sort(this->index.begin(), this->index.end(), [](const Entry& a, const Entry& b) { return a.id < b.id; });
    return true;
}

std::string TitlePack::read_string(uint32_t offset) const
{
    if (!offset || this->strings + offset >= this->file_size) return "";
    char         buf[MAX_STRING];
    const size_t n = (size_t)std::min<uint64_t>(sizeof buf, this->file_size - this->strings - offset);
    if (fseeko(this->file, (off_t)(this->strings + offset), SEEK_SET) != 0) return "";
    const size_t got = std::fread(buf, 1, n, this->file);
    const void*  end = std::memchr(buf, 0, got);
    return safe_text(std::string(buf, end ? (const char*)end - buf : got));
}

std::optional<Title> TitlePack::find(uint64_t id) const
{
    const auto it = std::lower_bound(this->index.begin(), this->index.end(), id, [](const Entry& e, uint64_t v) { return e.id < v; });
    if (it == this->index.end() || it->id != id) return std::nullopt;
    Title                             t;
    const std::lock_guard<std::mutex> g(this->lock);
    if (it->flags & HAS_NAME) t.name = this->read_string(it->name);
    if (it->flags & HAS_PUBLISHER) t.publisher = this->read_string(it->publisher);
    if (it->flags & HAS_VERSION) t.version = it->version;
    if (it->flags & HAS_DATE) t.release_date = it->release_date;
    if (it->flags & HAS_SIZE) t.size = it->size;
    if (t.name.empty() && t.publisher.empty() && !t.version && !t.release_date && !t.size) return std::nullopt;
    return t;
}

void TitlePack::each_base_name(const std::function<bool(uint64_t, const std::string&)>& each) const
{
    for (const Entry& e : this->index) {
        // A base game: its id ends in 000, bit 12 clear (an add-on's set).
        if ((e.id & 0xfff) || ((e.id >> 12) & 1) || !(e.flags & HAS_NAME)) continue;
        std::string name;
        {
            const std::lock_guard<std::mutex> g(this->lock);
            name = this->read_string(e.name);
        }
        if (!name.empty() && !each(e.id, name)) return;
    }
}

IconPack::~IconPack()
{
    if (this->file) std::fclose(this->file);
}

bool IconPack::open(const std::string& path, std::string* why)
{
    this->index.clear();
    if (this->file) std::fclose(this->file);
    this->file = std::fopen(path.c_str(), "rb");
    if (!this->file) {
        if (why) *why = "cannot be opened";
        return false;
    }
    uint32_t count = 0;
    if (!header(this->file, ICON_MAGIC, ICON_ENTRY, &count, &this->data, why)) return false;
    const uint64_t file_size = size_of(this->file);
    const uint64_t table = (uint64_t)count * ICON_ENTRY;
    if (table > MAX_INDEX || this->data < HEADER + table || this->data > file_size) {
        if (why) *why = "an index that does not hold together";
        return false;
    }
    std::vector<uint8_t> raw((size_t)table);
    if (!read_at(this->file, HEADER, raw.data(), raw.size())) {
        if (why) *why = "too short";
        return false;
    }
    this->index.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* p = raw.data() + (size_t)i * ICON_ENTRY;
        Entry          e{};
        e.id = le64(p);
        e.offset = le64(p + 8);
        e.size = le32(p + 16);
        std::memcpy(e.ext, p + 20, sizeof e.ext);
        e.ext[sizeof e.ext - 1] = 0;
        // Within the file, and an icon's size.
        const uint64_t start = this->data + e.offset;
        if (!e.id || !e.size || e.size > MAX_ICON || start < this->data || start + e.size > file_size) continue;
        this->index.push_back(e);
    }
    std::stable_sort(this->index.begin(), this->index.end(), [](const Entry& a, const Entry& b) { return a.id < b.id; });
    return true;
}

const IconPack::Entry* IconPack::entry_of(uint64_t id) const
{
    const auto it = std::lower_bound(this->index.begin(), this->index.end(), id, [](const Entry& e, uint64_t v) { return e.id < v; });
    return it != this->index.end() && it->id == id ? &*it : nullptr;
}

bool IconPack::has(uint64_t id) const
{
    return this->entry_of(id) != nullptr;
}

bool IconPack::find(uint64_t id, std::vector<uint8_t>* bytes, std::string* ext) const
{
    const Entry* e = this->entry_of(id);
    if (!e) return false;
    bytes->resize(e->size);
    const std::lock_guard<std::mutex> g(this->lock);
    if (!read_at(this->file, this->data + e->offset, bytes->data(), bytes->size())) return false;
    if (ext) {
        *ext = e->ext;
        for (char& c : *ext) c = (char)std::tolower((unsigned char)c);
    }
    return true;
}

}   // namespace titledb
