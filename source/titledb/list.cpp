// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "titledb/list.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include <aurore/extern/nlohmann/json.hpp>

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

// "20231220" or 20231220 → 20231220; 0 for anything that is no date.
uint32_t date_of(uint64_t v)
{
    return v >= 19700101 && v <= 29991231 ? (uint32_t)v : 0;
}

bool number_of(const std::string& v, uint64_t* out)
{
    if (v.empty() || v.size() > 20) return false;
    char*                    end = nullptr;
    const unsigned long long n = std::strtoull(v.c_str(), &end, 10);
    if (!end || *end) return false;
    *out = n;
    return true;
}

// The list read as it streams: the keys one level in are title ids (two,
// under a Tinfoil index's "titledb"), the fields one level further what is
// known of each.
struct Reader : nlohmann::json_sax<nlohmann::json>
{
    std::vector<ListEntry>* out;
    int                     depth = 0;
    int                     base = 1;   // the depth of the title ids
    bool                    top_object = false, too_many = false;
    std::string             field;
    ListEntry               now;

    explicit Reader(std::vector<ListEntry>* o) : out(o) {}

    void take_number(uint64_t v)
    {
        if (this->depth != this->base + 1) return;
        if ((this->field == "Version" || this->field == "version") && v <= UINT32_MAX)
            this->now.version = (uint32_t)v, this->now.has_version = true;
        else if (this->field == "Size" || this->field == "size") this->now.size = v;
        else if (this->field == "releaseDate") this->now.release_date = date_of(v);
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
        if (this->depth != this->base + 1) return true;
        if (this->field == "Game Name" || this->field == "name") this->now.name = safe_text(v);
        else if (this->field == "publisher") this->now.publisher = safe_text(v);
        else if (uint64_t n = 0; number_of(v, &n)) this->take_number(n);
        return true;
    }
    bool binary(binary_t&) override { return true; }
    bool start_object(std::size_t) override
    {
        if (this->depth == 0) this->top_object = true;
        if (this->depth == this->base) {
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
        if (this->depth == this->base && this->now.id) {
            if (this->out->size() >= MAX_LIST_ENTRIES) return !(this->too_many = true);
            this->out->push_back(this->now);
            this->now.id = 0;
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
        // A Tinfoil index: its titles one level further in.
        if (this->depth == 1 && k == "titledb") this->base = 2;
        if (this->depth == this->base) this->now.id = title_id(k);
        else if (this->depth == this->base + 1) this->field = k;
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
    std::istringstream in(text);
    return parse_title_list(in, out, why);
}

bool parse_title_list(std::istream& in, std::vector<ListEntry>* out, std::string* why)
{
    out->clear();
    Reader     r(out);
    const bool ok = nlohmann::json::sax_parse(in, &r, nlohmann::json::input_format_t::json, false);
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
    // A string's offset, 0 for none.
    auto add = [&strings](const std::string& text) -> uint32_t {
        if (text.empty() || strings.size() + text.size() + 1 >= UINT32_MAX) return 0;
        const uint32_t at = (uint32_t)strings.size();
        strings += text;
        strings += '\0';
        return at;
    };
    for (const ListEntry& e : entries) {
        const uint32_t name = add(e.name), publisher = add(e.publisher);
        uint32_t       flags = 0;
        if (name) flags |= 1u << 0;
        if (publisher) flags |= 1u << 1;
        if (e.size) flags |= 1u << 4;
        if (e.has_version) flags |= 1u << 5;
        if (e.release_date) flags |= 1u << 6;
        put64(&table, e.id);
        put32(&table, name);
        put32(&table, publisher);
        put32(&table, 0);   // intro
        put32(&table, 0);   // description
        put64(&table, e.size);
        put32(&table, e.version);
        put32(&table, e.release_date);
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
