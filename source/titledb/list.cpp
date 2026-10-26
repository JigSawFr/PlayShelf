// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "titledb/list.hpp"

#include <algorithm>
#include <cstdlib>

#include <borealis/extern/nlohmann/json.hpp>

#include "titledb/pack.hpp"

namespace titledb
{
namespace
{
// "0100000000010000" (16 hex digits) → its id; 0 for anything else.
uint64_t title_id(const std::string& key)
{
    if (key.size() != 16) return 0;
    uint64_t v = 0;
    for (char c : key) {
        const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return 0;
        v = v << 4 | (uint64_t)d;
    }
    return v;
}

// The list read as it streams: the keys at depth 1 are title ids, the
// fields at depth 2 what is known of each.
struct Reader : nlohmann::json_sax<nlohmann::json>
{
    std::vector<ListEntry>* out;
    int                     depth = 0;
    bool                    top_object = false, too_many = false;
    std::string             field;
    ListEntry               now;

    explicit Reader(std::vector<ListEntry>* o) : out(o) {}

    void take_number(uint64_t v)
    {
        if (this->depth != 2) return;
        if (this->field == "Version" && v <= UINT32_MAX) this->now.version = (uint32_t)v, this->now.has_version = true;
        else if (this->field == "Size") this->now.size = v;
    }
    bool null() override { return true; }
    bool boolean(bool) override { return true; }
    bool number_integer(number_integer_t v) override
    {
        if (v >= 0) this->take_number((uint64_t)v);
        return true;
    }
    bool number_unsigned(number_unsigned_t v) override
    {
        this->take_number(v);
        return true;
    }
    bool number_float(number_float_t, const string_t&) override { return true; }
    bool string(string_t& v) override
    {
        if (this->depth != 2) return true;
        if (this->field == "Game Name" || this->field == "name" || this->field == "Name") this->now.name = safe_text(v);
        else if (this->field == "Version" || this->field == "version") {
            char*                    end = nullptr;
            const unsigned long long n = std::strtoull(v.c_str(), &end, 10);
            if (!v.empty() && end && !*end && n <= UINT32_MAX) this->now.version = (uint32_t)n, this->now.has_version = true;
        } else if (this->field == "Size" || this->field == "size") {
            char*                    end = nullptr;
            const unsigned long long n = std::strtoull(v.c_str(), &end, 10);
            if (!v.empty() && end && !*end) this->now.size = n;
        }
        return true;
    }
    bool binary(binary_t&) override { return true; }
    bool start_object(std::size_t) override
    {
        if (this->depth == 0) this->top_object = true;
        if (this->depth == 1) {
            const uint64_t id = this->now.id;
            this->now = {};
            this->now.id = id;
        }
        this->depth++;
        return true;
    }
    bool end_object() override
    {
        this->depth--;
        if (this->depth == 1 && this->now.id) {
            if (this->out->size() >= MAX_LIST_ENTRIES) return !(this->too_many = true);
            this->out->push_back(this->now);
        }
        return true;
    }
    bool start_array(std::size_t) override
    {
        this->depth++;
        return true;
    }
    bool end_array() override
    {
        this->depth--;
        return true;
    }
    bool key(string_t& k) override
    {
        if (this->depth == 1) this->now.id = title_id(k);
        else if (this->depth == 2) this->field = k;
        return true;
    }
    bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override { return false; }
};

void put32(std::string* b, uint32_t v)
{
    for (int i = 0; i < 4; i++) b->push_back((char)(v >> (8 * i)));
}

void put64(std::string* b, uint64_t v)
{
    put32(b, (uint32_t)v);
    put32(b, (uint32_t)(v >> 32));
}
}   // namespace

bool parse_title_list(const std::string& text, std::vector<ListEntry>* out, std::string* why)
{
    out->clear();
    Reader     r(out);
    const bool ok = nlohmann::json::sax_parse(text, &r, nlohmann::json::input_format_t::json, false);
    if (!ok || !r.top_object) {
        if (why) *why = r.too_many ? "too many titles" : "not a title list";
        out->clear();
        return false;
    }
    // By id, the last of an id kept.
    std::stable_sort(out->begin(), out->end(), [](const ListEntry& a, const ListEntry& b) { return a.id < b.id; });
    std::vector<ListEntry> unique;
    unique.reserve(out->size());
    for (ListEntry& e : *out) {
        if (!unique.empty() && unique.back().id == e.id) unique.back() = std::move(e);
        else unique.push_back(std::move(e));
    }
    *out = std::move(unique);
    if (out->empty()) {
        if (why) *why = "no title in the list";
        return false;
    }
    return true;
}

std::string titles_pack_of(const std::vector<ListEntry>& entries)
{
    std::string strings(1, '\0'), table;
    table.reserve(entries.size() * 48);
    for (const ListEntry& e : entries) {
        uint32_t flags = 0, name = 0;
        if (!e.name.empty() && strings.size() + e.name.size() + 1 < UINT32_MAX) {
            name = (uint32_t)strings.size();
            strings += e.name;
            strings += '\0';
            flags |= 1u << 0;
        }
        if (e.size) flags |= 1u << 4;
        if (e.has_version) flags |= 1u << 5;
        put64(&table, e.id);
        put32(&table, name);
        put32(&table, 0);   // publisher
        put32(&table, 0);   // intro
        put32(&table, 0);   // description
        put64(&table, e.size);
        put32(&table, e.version);
        put32(&table, 0);   // release date
        put32(&table, 0);   // is a demo
        put32(&table, flags);
    }
    std::string out = "CFTITLE1";
    put32(&out, 1);
    put32(&out, 48);
    put32(&out, (uint32_t)entries.size());
    put32(&out, 0);
    put64(&out, 32 + (uint64_t)table.size());
    return out + table + strings;
}

}   // namespace titledb
