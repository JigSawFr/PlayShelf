// install/verify_title.cpp: an installed title read back: every content of its record found, of the
// size the record says, and hashing to its id; what is not so is named; nothing is written.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <algorithm>
#include <map>
#include <set>

#include "check.h"
#include "install/recompress_meta.hpp"
#include "install/verify_title.hpp"
#include "util/sha256.hpp"

using namespace install;

namespace
{
using Bytes = std::vector<uint8_t>;

Bytes stream(size_t n, uint8_t seed)
{
    Bytes b(n);
    for (size_t i = 0; i < n; i++) b[i] = (uint8_t)(seed * 31 + i * 7 + (i >> 8));
    return b;
}

struct Fake : Console
{
    std::map<ContentId, Bytes> contents;
    std::map<std::string, Bytes> metas;
    std::set<ContentId>        unreadable;   // reads fail
    int                        writes = 0, reads = 0;

    static std::string key(const MetaKey& k) { return std::to_string(k.id) + ":" + std::to_string(k.version); }

    bool has_content(const ContentId& id) override { return contents.count(id) != 0; }
    Done create_placeholder(const ContentId&, uint64_t) override { writes++; return {}; }
    Done write_placeholder(const ContentId&, uint64_t, const uint8_t*, size_t) override { writes++; return {}; }
    Done register_placeholder(const ContentId&) override { writes++; return {}; }
    bool has_placeholder(const ContentId&) override { return false; }
    void delete_placeholder(const ContentId&) override { writes++; }
    std::vector<ContentId> placeholders() override { return {}; }
    void delete_content(const ContentId&) override { writes++; }
    uint64_t free_space() override { return 1ull << 30; }
    std::optional<Bytes> get_meta(const MetaKey& k) override
    {
        const auto it = metas.find(key(k));
        if (it == metas.end()) return std::nullopt;
        return it->second;
    }
    Done set_meta(const MetaKey&, const Bytes&) override { writes++; return {}; }
    Done remove_meta(const MetaKey&) override { writes++; return {}; }
    Done commit_meta() override { writes++; return {}; }
    Done push_application_record(uint64_t, const MetaKey&) override { writes++; return {}; }
    Done import_ticket(const Bytes&, const Bytes&) override { writes++; return {}; }
    uint32_t system_version() override { return 0x0c0000; }
    std::optional<uint64_t> content_size(const ContentId& id) override
    {
        const auto it = contents.find(id);
        if (it == contents.end()) return std::nullopt;
        return it->second.size();
    }
    Done read_content(const ContentId& id, uint64_t offset, std::span<uint8_t> out) override
    {
        reads++;
        if (unreadable.count(id)) return fail("a read failed");
        const Bytes& b = contents.at(id);
        if (offset > b.size() || out.size() > b.size() - offset) return fail("out of range");
        std::copy_n(b.begin() + (long)offset, out.size(), out.begin());
        return {};
    }
};

ContentId id_of_bytes(const Bytes& b)
{
    Sha256 h;
    h.update(b);
    return content_id_of(h.digest());
}

struct Title
{
    Fake                  console;
    MetaKey               key{0x0100000000010000ull, 0, MetaType::Application};
    std::vector<ContentId> ids;   // the content meta's, the program's, the control's
};

// A title of three contents (a content meta, a program of 2.5 pieces, a control), installed.
Title installed()
{
    Title t;
    const Bytes meta = stream(5000, 1), program = stream((size_t)(2.5 * (1 << 20)), 2), control = stream(70000, 3);
    ContentMeta m;
    m.title_id = t.key.id;
    m.type = MetaType::Application;
    m.extended_header.assign(16, 0);
    ContentInfo mc, p, c;
    mc.id = id_of_bytes(meta);
    mc.size = meta.size();
    mc.type = ContentType::Meta;
    p.id = id_of_bytes(program);
    p.size = program.size();
    p.type = ContentType::Program;
    c.id = id_of_bytes(control);
    c.size = control.size();
    c.type = ContentType::Control;
    m.contents = {p, c};
    t.console.contents = {{mc.id, meta}, {p.id, program}, {c.id, control}};
    t.console.metas[Fake::key(t.key)] = cnmt_install_record(m, mc);
    t.ids = {mc.id, p.id, c.id};
    return t;
}

ContentState state_of(const TitleVerdict& v, const ContentId& id)
{
    for (const ContentVerdict& c : v.contents)
        if (c.id == id) return c.state;
    return ContentState::Missing;
}
}   // namespace

int main()
{
    {   // a title as installed: every content Ok, in the record's order, read whole, nothing written
        Title    t = installed();
        uint64_t done = 0, total = 0;
        const auto v = verify_title(t.console, t.key, [&](uint64_t d, uint64_t all) { done = d; total = all; });
        if (!CHECK(v.has_value())) return 1;
        CHECK(v->ok() && v->damaged().empty() && v->contents.size() == 3);
        CHECK(v->contents[0].id == t.ids[0] && v->contents[0].type == ContentType::Meta);   // the content meta first
        CHECK(done == total && total == 5000 + (size_t)(2.5 * (1 << 20)) + 70000);
        CHECK(t.console.writes == 0);
    }
    {   // a content that is gone
        Title t = installed();
        t.console.contents.erase(t.ids[2]);
        const auto v = verify_title(t.console, t.key);
        if (!CHECK(v.has_value())) return 1;
        CHECK(!v->ok() && state_of(*v, t.ids[2]) == ContentState::Missing && state_of(*v, t.ids[1]) == ContentState::Ok);
        CHECK(v->damaged() == std::vector<ContentId>{t.ids[2]});
    }
    {   // a content of another size (cut short), and one with a byte changed in its last piece
        Title t = installed();
        t.console.contents[t.ids[2]].resize(1000);
        t.console.contents[t.ids[1]].back() ^= 1;
        const auto v = verify_title(t.console, t.key);
        if (!CHECK(v.has_value())) return 1;
        CHECK(state_of(*v, t.ids[2]) == ContentState::SizeWrong);
        CHECK(state_of(*v, t.ids[1]) == ContentState::Corrupt);
        CHECK(state_of(*v, t.ids[0]) == ContentState::Ok);
        CHECK(v->damaged().size() == 2);
    }
    {   // a read that fails: that content is unreadable, the others are still checked
        Title t = installed();
        t.console.unreadable.insert(t.ids[1]);
        const auto v = verify_title(t.console, t.key);
        if (!CHECK(v.has_value())) return 1;
        CHECK(state_of(*v, t.ids[1]) == ContentState::Unreadable && state_of(*v, t.ids[2]) == ContentState::Ok);
    }
    {   // a title that is not installed, a record cut short
        Title t = installed();
        CHECK(!verify_title(t.console, MetaKey{0x0100000000099999ull, 0, MetaType::Application}).has_value());
        t.console.metas[Fake::key(t.key)].resize(5);
        CHECK(!verify_title(t.console, t.key).has_value());
    }
    {   // cancelled: an error, no verdict
        Title             t = installed();
        std::atomic<bool> cancel{false};
        int               reads = 0;
        const auto        v = verify_title(t.console, t.key, [&](uint64_t, uint64_t) { if (++reads == 2) cancel = true; }, &cancel);
        CHECK(!v.has_value() && v.error() == "cancelled");
        CHECK(t.console.writes == 0);
    }
    CHECK_DONE("verify title: a title read back, what is missing, wrong, unreadable or corrupt named, nothing written passed");
}
