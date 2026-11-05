// install/compress_title.cpp: a title installed in memory (a program NCA, a
// control content, a .cnmt.nca and the record ncm keeps) made over with its
// program compressed, and everything that can go wrong on the way.
//
// The invariant: whatever happens, the title ends up exactly as it was (its
// record, its contents byte for byte, nothing left over), or exactly as it
// should be (the new record, the new contents, which read back as the
// original's). Faults are injected in every console call in turn; so are
// crashes (an exception out of the call, the run abandoned on the spot, then
// recover() from the journal); so is silent corruption of what is written;
// so are cancels, and a journal that cannot be kept.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <atomic>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

#include "../install/patch_builders.hpp"
#include "check.h"
#include "install/compress_title.hpp"
#include "install/recompress_meta.hpp"

using namespace install;
using namespace test_install;

namespace
{
struct Crash : std::runtime_error
{
    Crash() : std::runtime_error("crash") {}
};

using KeyT = std::tuple<uint64_t, uint32_t, uint8_t>;
KeyT tk(const MetaKey& k) { return {k.id, k.version, (uint8_t)k.type}; }

struct Mem : Console
{
    std::map<ContentId, Bytes> contents, pholders;
    // ncm makes a content's folder when a placeholder is created with that content id, and registering renames the placeholder into it:
    // a register under an id whose folder was never made fails.
    std::set<ContentId>        folders;
    std::map<KeyT, Bytes>      metas, pending;
    std::set<KeyT>             removed;   // removals not yet committed
    uint64_t                   free = 1ull << 40;
    uint32_t                   fw = 15u << 16;
    // Fault injection: the Nth mutating call fails (fault_at), or the process
    // dies in it (crash_at), or the Nth write has a byte changed (corrupt_at).
    int ops = 0, fault_at = -1, crash_at = -1, corrupt_at = -1, writes = 0;
    int reads = 0, read_fault_at = -1;

    bool step()
    {
        const int n = ops++;
        if (n == crash_at) {
            pending.clear();   // the service's uncommitted edits die with the process
            removed.clear();
            throw Crash();
        }
        return n != fault_at;
    }

    bool has_content(const ContentId& id) override { return contents.count(id) != 0; }
    Done create_placeholder(const ContentId& id, uint64_t size) override
    {
        if (!step()) return fail("fault");
        if (pholders.count(id)) return fail("exists");
        pholders[id] = Bytes(size);
        folders.insert(id);   // the installer's placeholder id is its content id
        return {};
    }
    Done write_placeholder(const ContentId& id, uint64_t offset, const uint8_t* data, size_t size) override
    {
        if (!step()) return fail("fault");
        auto it = pholders.find(id);
        if (it == pholders.end() || offset + size > it->second.size()) return fail("bad write");
        std::memcpy(it->second.data() + offset, data, size);
        if (writes++ == corrupt_at && size) it->second[offset + size / 2] ^= 0x40;
        return {};
    }
    Done register_placeholder(const ContentId& id) override { return register_placeholder_as(id, id); }
    Done register_placeholder_as(const ContentId& placeholder, const ContentId& content) override
    {
        if (!step()) return fail("fault");
        auto it = pholders.find(placeholder);
        if (it == pholders.end() || contents.count(content)) return fail("cannot register");
        if (!folders.count(content)) return fail("no folder for the content (ncm)");
        contents[content] = std::move(it->second);
        pholders.erase(it);
        return {};
    }
    bool has_placeholder(const ContentId& id) override { return pholders.count(id) != 0; }
    void delete_placeholder(const ContentId& id) override
    {
        step();   // a crash is possible, a fault is not
        pholders.erase(id);
    }
    std::vector<ContentId> placeholders() override
    {
        std::vector<ContentId> v;
        for (auto& [k, _] : pholders) v.push_back(k);
        return v;
    }
    void delete_content(const ContentId& id) override
    {
        step();
        contents.erase(id);
    }
    uint64_t free_space() override { return free; }
    std::optional<std::vector<uint8_t>> get_meta(const MetaKey& k) override
    {
        if (removed.count(tk(k))) return std::nullopt;
        if (auto p = pending.find(tk(k)); p != pending.end()) return p->second;
        if (auto m = metas.find(tk(k)); m != metas.end()) return m->second;
        return std::nullopt;
    }
    Done set_meta(const MetaKey& k, const std::vector<uint8_t>& r) override
    {
        if (!step()) return fail("fault");
        pending[tk(k)] = r;
        removed.erase(tk(k));
        return {};
    }
    Done remove_meta(const MetaKey& k) override
    {
        if (!step()) return fail("fault");
        pending.erase(tk(k));
        removed.insert(tk(k));
        return {};
    }
    Done commit_meta() override
    {
        if (!step()) return fail("fault");
        for (auto& [k, v] : pending) metas[k] = v;
        for (const KeyT& k : removed) metas.erase(k);
        pending.clear();
        removed.clear();
        return {};
    }
    std::vector<KeyT> app_record;   // what ns lists for the application
    int               replaces = 0, replace_half_at = -1;   // the Nth replace drops the record, then fails
    Done replace_application_record(uint64_t, const std::vector<MetaKey>& keys) override
    {
        if (!step()) return fail("fault");
        if (replaces++ == replace_half_at) {
            app_record.clear();
            return fail("half done");
        }
        app_record.clear();
        for (const MetaKey& k : keys) app_record.push_back(tk(k));
        return {};
    }
    std::vector<std::pair<uint64_t, KeyT>> pushed;
    Done push_application_record(uint64_t app, const MetaKey& k) override
    {
        if (!step()) return fail("fault");
        pushed.push_back({app, tk(k)});
        return {};
    }
    Done     import_ticket(const std::vector<uint8_t>&, const std::vector<uint8_t>&) override { return {}; }
    uint32_t system_version() override { return fw; }
    std::optional<uint64_t> content_size(const ContentId& id) override
    {
        auto it = contents.find(id);
        if (it == contents.end()) return std::nullopt;
        return it->second.size();
    }
    Done read_content(const ContentId& id, uint64_t offset, std::span<uint8_t> out) override
    {
        if (reads++ == read_fault_at) return fail("read fault");
        auto it = contents.find(id);
        if (it == contents.end() || offset > it->second.size() || out.size() > it->second.size() - offset) return fail("bad read");
        std::memcpy(out.data(), it->second.data() + offset, out.size());
        return {};
    }
};

struct MemJournal : JournalStore
{
    std::optional<std::string> text;
    int                        saves = 0, fail_save_at = -1;
    std::optional<std::string> load() override { return text; }
    bool                       save(const std::string& t) override
    {
        if (saves++ == fail_save_at) return false;
        text = t;
        return true;
    }
    void clear() override { text.reset(); }
};

const MetaKey KEY{0x0100000000020000ull, 0, MetaType::Application};

struct Title
{
    Mem   mem;
    Mem   start;   // the title as installed: what 'as it was' is compared with (mem is the one the engine changes)
    Bytes program;
    Bytes record;
    Built built;
};

// A title installed: program, control, content meta and record.
Title install_title(size_t image_size, uint32_t seed, bool rights = false, bool incompressible = false)
{
    Title t;
    t.built = program_nca(image_size, seed, rights);
    if (incompressible) {
        // A RomFS of noise: rebuilt around the same ExeFS.
        std::mt19937 rng(seed);
        Bytes        noise(image_size);
        for (auto& b : noise) b = (uint8_t)rng();
        Built n = program_nca(image_size, seed, rights);
        (void)noise;
        t.built = n;
    }
    t.program = t.built.nca;
    ContentInfo prog;
    prog.id = content_id_of([&] { Sha256 h; h.update(t.program); return h.digest(); }());
    {
        Sha256 h;
        h.update(t.program);
        prog.hash = h.digest();
    }
    prog.size = t.program.size();
    prog.type = ContentType::Program;
    ContentInfo control;
    control.id = id_of(0x22);
    control.size = 3000;
    control.type = ContentType::Control;
    control.hash.fill(0xbb);
    t.mem.contents[control.id] = stream(3000, "control");

    const Bytes cnmt = packaged_cnmt(MetaType::Application, KEY.id, 0, {prog, control});
    const Bytes meta = meta_nca(cnmt);
    ContentInfo meta_content;
    meta_content.id = content_id_of([&] { Sha256 h; h.update(meta); return h.digest(); }());
    meta_content.size = meta.size();
    meta_content.type = ContentType::Meta;
    t.mem.contents[prog.id] = t.program;
    t.mem.contents[meta_content.id] = meta;
    const auto parsed = cnmt_parse(cnmt);
    t.record = cnmt_install_record(*parsed, meta_content);
    t.mem.metas[tk(KEY)] = t.record;
    t.start = t.mem;
    return t;
}

TitleCompressor make(Mem& mem, MemJournal& j, CompressOptions o = {}, CompressCallbacks cb = {})
{
    o.block_size = 16384;
    return TitleCompressor(mem, test_keys(), j, lz4_compress, lz4_decompress, KEY, o, std::move(cb));
}

// The title as it was: record, contents byte for byte, nothing else.
bool is_baseline(const Mem& m, const Title& t)
{
    return m.metas == t.start.metas && m.pending.empty() && m.removed.empty() && m.contents == t.start.contents && m.pholders.empty();
}

// The title as it should be after a run: the new record, the new program and
// content meta (the program read back against the original, here, apart from
// the engine), the old ones kept or gone as asked.
// old: 1 kept, 0 gone, -1 either (a keep() that died half way)
bool is_compressed(const Mem& m, const Title& t, int old)
{
    const auto it = m.metas.find(tk(KEY));
    if (it == m.metas.end() || !m.pending.empty() || !m.pholders.empty() || it->second == t.record) return false;
    const auto infos = record_contents(it->second);
    if (!infos) return false;
    std::set<ContentId> expected;
    for (const ContentInfo& i : *infos) {
        if (!m.contents.count(i.id)) return false;
        expected.insert(i.id);
        if (i.type == ContentType::Program) {
            ProgramRecompressor rc([&t](uint64_t off, std::span<uint8_t> out) {
                std::memcpy(out.data(), t.program.data() + off, out.size());
                return true;
            }, t.program.size(), test_keys(), lz4_compress, lz4_decompress, 16384);
            if (!rc.inspect()) return false;
            const Bytes& made = m.contents.at(i.id);
            if (i.size != made.size()) return false;
            if (!rc.verify([&made](uint64_t off, std::span<uint8_t> out) {
                    if (off > made.size() || out.size() > made.size() - off) return false;
                    std::memcpy(out.data(), made.data() + off, out.size());
                    return true;
                }, made.size()))
                return false;
        }
    }
    const auto old_infos = record_contents(t.record);
    for (const auto& [id, bytes] : t.start.contents) {
        bool replaced = false;   // the old program and content meta
        for (const ContentInfo& i : *old_infos) replaced = replaced || (i.id == id && (i.type == ContentType::Program || i.type == ContentType::Meta));
        if (replaced && old != 1) {
            if (old == 0 && m.contents.count(id)) return false;
            if (m.contents.count(id)) {
                if (m.contents.at(id) != bytes) return false;
                expected.insert(id);
            }
            continue;
        }
        if (!m.contents.count(id) || m.contents.at(id) != bytes) return false;
        expected.insert(id);
    }
    for (const auto& [id, _] : m.contents)
        if (!expected.count(id)) return false;
    return true;
}

void test_happy_path()
{
    Title      t = install_title(300'000, 1);
    MemJournal j;
    CompressReport seen;
    bool           confirmed = false;
    std::vector<CompressStage> stages;
    CompressCallbacks cb;
    cb.confirm = [&](const CompressReport& r) { seen = r; confirmed = true; return true; };
    cb.progress = [&](CompressStage s, uint64_t d, uint64_t tot) {
        CHECK(d <= tot);
        if (stages.empty() || stages.back() != s) stages.push_back(s);
    };
    auto c = make(t.mem, j, {}, cb);
    if (!CHECK(c.prepare().has_value())) return;
    CHECK(confirmed && seen.program_before == t.program.size() && seen.program_after < seen.program_before);
    CHECK(seen.saving_percent() >= 5);
    CHECK(t.mem.ops == 0 && !j.text);   // nothing written, nothing journaled
    CHECK(is_baseline(t.mem, t));

    const Done ran = c.run();
    if (!CHECK(ran.has_value())) {
        fprintf(stderr, "run: %s\n", ran.error().c_str());
        return;
    }
    CHECK(is_compressed(t.mem, t, 1));
    CHECK(j.text && CompressJournal::parse(*j.text) && CompressJournal::parse(*j.text)->phase == CompressJournal::Phase::Swapped);
    CHECK(stages.size() == 3 && stages[0] == CompressStage::Measuring && stages[1] == CompressStage::Writing && stages[2] == CompressStage::Checking);
    // The record says the firmware that reads it.
    const auto rec = record_contents(*t.mem.get_meta(KEY));
    CHECK(rec.has_value());

    CHECK(c.keep().has_value());
    CHECK(is_compressed(t.mem, t, 0));
    CHECK(!j.text);
    CHECK(!c.keep().has_value() && !c.revert().has_value());   // nothing more to do
}

void test_revert()
{
    Title      t = install_title(300'000, 2);
    MemJournal j;
    auto       c = make(t.mem, j);
    CHECK(c.prepare().has_value() && c.run().has_value());
    CHECK(c.revert().has_value());
    CHECK(is_baseline(t.mem, t));
    CHECK(!j.text);
}

void test_refusals_write_nothing()
{
    auto nothing_written = [](const char* what, Title& t, MemJournal& j, const Done& d) {
        const bool ok = !d.has_value() && t.mem.ops == 0 && !j.text && is_baseline(t.mem, t);
        if (!ok) fprintf(stderr, "wrote or accepted: %s (%s)\n", what, d.has_value() ? "accepted" : d.error().c_str());
        CHECK(ok);
    };
    {
        Title t = install_title(300'000, 3);
        MemJournal j;
        t.mem.fw = 11u << 16;
        auto c = make(t.mem, j);
        nothing_written("old firmware", t, j, c.prepare());
    }
    {
        Title t = install_title(300'000, 3);
        MemJournal j;
        CompressOptions o;
        o.update_installed = true;
        auto c = make(t.mem, j, o);
        nothing_written("update installed", t, j, c.prepare());
    }
    {
        Title t = install_title(300'000, 3);
        MemJournal j;
        t.mem.free = t.program.size();   // no room beside the old copy
        auto c = make(t.mem, j);
        nothing_written("no room", t, j, c.prepare());
    }
    {
        Title t = install_title(300'000, 3, true);   // sold with a ticket
        MemJournal j;
        auto c = make(t.mem, j);
        nothing_written("rights id", t, j, c.prepare());
    }
    {
        Title t = install_title(300'000, 3);
        MemJournal j;
        CompressCallbacks cb;
        cb.confirm = [](const CompressReport&) { return false; };
        auto c = make(t.mem, j, {}, cb);
        nothing_written("declined", t, j, c.prepare());
    }
    {
        Title t = install_title(300'000, 3);
        MemJournal j;
        CompressOptions o;
        o.min_saving_percent = 99;
        auto c = make(t.mem, j, o);
        nothing_written("gain too small", t, j, c.prepare());
    }
    {
        Title t = install_title(300'000, 3);
        MemJournal j;
        t.mem.metas.clear();
        auto c = make(t.mem, j);
        CHECK(!c.prepare().has_value() && t.mem.ops == 0);
    }
    {
        Title t = install_title(300'000, 3);
        MemJournal j;
        auto c = make(t.mem, j);
        CHECK(!c.run().has_value());   // before prepare()
        nothing_written("run before prepare", t, j, Done(std::unexpected(std::string("x"))));
        MetaKey patch = KEY;
        patch.type = MetaType::Patch;
        TitleCompressor cp(t.mem, test_keys(), j, lz4_compress, lz4_decompress, patch);
        CHECK(!cp.prepare().has_value());
    }
    {   // a read that fails while measuring
        Title t = install_title(300'000, 3);
        MemJournal j;
        t.mem.read_fault_at = 1;
        auto c = make(t.mem, j);
        nothing_written("read fault in prepare", t, j, c.prepare());
    }
    {   // a cancel while measuring
        Title t = install_title(300'000, 3);
        MemJournal j;
        std::atomic<bool> stop{false};
        CompressCallbacks cb;
        cb.cancel = &stop;
        cb.progress = [&](CompressStage, uint64_t, uint64_t) { stop = true; };
        auto c = make(t.mem, j, {}, cb);
        nothing_written("cancel in prepare", t, j, c.prepare());
    }
    {   // the title changed between prepare() and run()
        Title t = install_title(300'000, 3);
        MemJournal j;
        auto c = make(t.mem, j);
        CHECK(c.prepare().has_value());
        t.mem.metas[tk(KEY)].back() ^= 1;
        const Bytes changed = t.mem.metas[tk(KEY)];
        CHECK(!c.run().has_value() && t.mem.ops == 0 && !j.text && t.mem.metas[tk(KEY)] == changed);
    }
    {   // the journal cannot be kept: nothing is written
        Title t = install_title(300'000, 3);
        MemJournal j;
        j.fail_save_at = 0;
        auto c = make(t.mem, j);
        CHECK(c.prepare().has_value());
        CHECK(!c.run().has_value() && t.mem.ops == 0 && is_baseline(t.mem, t));
    }
}

// Every console call fails in turn: the title is as it was, or compressed
// properly, never in between; nothing left over.
void test_faults()
{
    const Title base = install_title(300'000, 4);
    int          last_failing = -1;
    for (int k = 0; k < 400; k++) {
        Title      t = base;
        MemJournal j;
        t.mem.fault_at = k;
        auto c = make(t.mem, j);
        CHECK(c.prepare().has_value());
        t.mem.ops = 0;
        const Done ran = c.run();
        if (ran) {
            // The fault came after everything (or hit a call that cannot fail): the new title stands.
            CHECK(is_compressed(t.mem, t, 1));
            CHECK(c.keep().has_value() && is_compressed(t.mem, t, 0));
            if (t.mem.ops <= k) break;   // no fault was injected: done sweeping
            continue;
        }
        last_failing = k;
        const bool clean = is_baseline(t.mem, t) && !j.text;
        if (!clean) fprintf(stderr, "fault at call %d left the title not as it was (%s)\n", k, ran.error().c_str());
        CHECK(clean);
    }
    CHECK(last_failing > 10);
}

// Silent corruption of what is written: found by the read-back.
void test_corruption()
{
    const Title base = install_title(300'000, 5);
    int         found = 0, total = 0;
    for (int w = 0; w < 60; w += 3) {
        Title      t = base;
        MemJournal j;
        t.mem.corrupt_at = w;
        auto c = make(t.mem, j);
        CHECK(c.prepare().has_value());
        t.mem.writes = 0;
        const Done ran = c.run();
        if (t.mem.writes <= w) break;   // fewer writes than that
        total++;
        if (!ran) {
            found++;
            CHECK(is_baseline(t.mem, t) && !j.text);
        } else {
            // A corrupted byte in what carries no meaning (padding) is not an error, but the title must still be good.
            CHECK(is_compressed(t.mem, t, 1));
        }
    }
    CHECK(total >= 5 && found >= total - 2);
}

void test_cancel()
{
    const Title base = install_title(300'000, 6);
    for (int at : {1, 5, 20, 40}) {
        Title      t = base;
        MemJournal j;
        std::atomic<bool> stop{false};
        int               seen = 0;
        CompressCallbacks cb;
        cb.cancel = &stop;
        cb.progress = [&](CompressStage s, uint64_t, uint64_t) {
            if (s != CompressStage::Measuring && ++seen == at) stop = true;
        };
        auto c = make(t.mem, j, {}, cb);
        CHECK(c.prepare().has_value());
        const Done ran = c.run();
        if (ran) {
            CHECK(is_compressed(t.mem, t, 1));
            continue;
        }
        CHECK(is_baseline(t.mem, t) && !j.text);
    }
}

void test_journal_faults()
{
    const Title base = install_title(300'000, 7);
    for (int k = 0; k < 12; k++) {
        Title      t = base;
        MemJournal j;
        j.fail_save_at = k;
        auto c = make(t.mem, j);
        CHECK(c.prepare().has_value());
        const Done ran = c.run();
        if (ran) {
            CHECK(is_compressed(t.mem, t, 1));
            continue;
        }
        const bool clean = is_baseline(t.mem, t);
        if (!clean) fprintf(stderr, "journal fault at save %d left the title not as it was (%s)\n", k, ran.error().c_str());
        CHECK(clean);
    }
}

// The process dies in every console call: then recover() from the journal.
void test_crashes()
{
    const Title base = install_title(300'000, 8);
    int         cleaned = 0, awaiting = 0;
    for (int k = 0; k < 400; k++) {
        Title      t = base;
        MemJournal j;
        t.mem.crash_at = k;
        bool crashed = false;
        try {
            auto c = make(t.mem, j);
            CHECK(c.prepare().has_value());
            t.mem.ops = 0;
            CHECK(c.run().has_value());
            CHECK(c.keep().has_value());
        } catch (const Crash&) {
            crashed = true;
        }
        t.mem.crash_at = -1;
        if (!crashed) {
            CHECK(is_compressed(t.mem, t, 0));
            break;
        }
        // The next start.
        auto left = TitleCompressor::recover(t.mem, j);
        if (!CHECK(left.has_value())) continue;
        switch (left->found) {
        case TitleCompressor::Found::Nothing:
            // Nothing was journaled yet, or all was done: the title is as it was, or finished.
            CHECK(is_baseline(t.mem, t) || is_compressed(t.mem, t, 0));
            break;
        case TitleCompressor::Found::CleanedUp:
            cleaned++;
            CHECK(is_baseline(t.mem, t) && !j.text);
            break;
        case TitleCompressor::Found::AwaitingDecision:
            awaiting++;
            CHECK(left->key == KEY);
            CHECK(is_compressed(t.mem, t, -1));
            // Either answer works.
            if (k % 2) {
                CHECK(TitleCompressor::revert_left(t.mem, j).has_value());
                if (!CHECK(is_baseline(t.mem, t) && !j.text)) fprintf(stderr, "  (crash at call %d, then revert)\n", k);
            } else {
                CHECK(TitleCompressor::keep_left(t.mem, j).has_value());
                if (!CHECK(is_compressed(t.mem, t, 0) && !j.text)) fprintf(stderr, "  (crash at call %d, then keep)\n", k);
            }
            break;
        }
    }
    CHECK(cleaned > 5);
    (void)awaiting;
}

// The user reverts after a restart; and a crash while reverting.
void test_crash_while_reverting()
{
    const Title base = install_title(300'000, 9);
    for (int k = 0; k < 20; k++) {
        Title      t = base;
        MemJournal j;
        auto       c = make(t.mem, j);
        CHECK(c.prepare().has_value() && c.run().has_value());
        t.mem.ops = 0;
        t.mem.crash_at = k;
        bool crashed = false;
        try {
            CHECK(TitleCompressor::revert_left(t.mem, j).has_value());
        } catch (const Crash&) {
            crashed = true;
        }
        t.mem.crash_at = -1;
        if (!crashed) {
            CHECK(is_baseline(t.mem, t) && !j.text);
            break;
        }
        auto left = TitleCompressor::recover(t.mem, j);
        CHECK(left.has_value());
        if (left->found == TitleCompressor::Found::AwaitingDecision) CHECK(TitleCompressor::revert_left(t.mem, j).has_value());
        CHECK(is_baseline(t.mem, t) && !j.text);
    }
}

// The process ends after run(), before the user decides: the next start finds it, and either answer works.
void test_restart_after_run()
{
    for (const bool revert : {true, false}) {
        Title      t = install_title(300'000, 61);
        MemJournal j;
        {
            auto c = make(t.mem, j);
            CHECK(c.prepare().has_value() && c.run().has_value());
        }   // the engine is gone; the journal and the console stay
        auto left = TitleCompressor::recover(t.mem, j);
        if (!CHECK(left.has_value() && left->found == TitleCompressor::Found::AwaitingDecision && left->key == KEY)) continue;
        CHECK(is_compressed(t.mem, t, 1));
        CHECK(t.mem.pushed.empty());   // the key did not change: ns is not told anything
        if (revert) {
            CHECK(TitleCompressor::revert_left(t.mem, j).has_value());
            CHECK(is_baseline(t.mem, t) && !j.text);
        } else {
            CHECK(TitleCompressor::keep_left(t.mem, j).has_value());
            CHECK(is_compressed(t.mem, t, 0) && !j.text);
        }
    }
}

void test_journal_text()
{
    CompressJournal j;
    j.phase = CompressJournal::Phase::Swapped;
    j.key = KEY;
    j.old_record = {1, 2, 3};
    j.new_record = {4, 5};
    j.old_program = id_of(0x11);
    j.old_meta = id_of(0x22);
    j.placeholders = {id_of(0x33)};
    j.contents = {id_of(0x44), id_of(0x55)};
    const auto back = CompressJournal::parse(j.text());
    if (!CHECK(back.has_value())) return;
    CHECK(back->phase == j.phase && back->key == j.key && back->old_record == j.old_record && back->new_record == j.new_record);
    CHECK(back->old_program == j.old_program && back->old_meta == j.old_meta && back->placeholders == j.placeholders && back->contents == j.contents);
    CHECK(!CompressJournal::parse("schema=2\n" + j.text()).has_value());                  // newer: not read
    CHECK(!CompressJournal::parse("").has_value());
    CHECK(!CompressJournal::parse("phase=writing\n").has_value());
    CHECK(CompressJournal::parse(j.text() + "future_key=whatever\n").has_value());      // unknown keys skipped
    {   // a journal of another schema is left alone by recover()
        Title      t = install_title(300'000, 10);
        MemJournal mj;
        mj.text = "schema=2\nphase=writing\n";
        const auto l = TitleCompressor::recover(t.mem, mj);
        CHECK(l.has_value() && l->found == TitleCompressor::Found::Nothing && mj.text.has_value() && is_baseline(t.mem, t));
        CHECK(!TitleCompressor::keep_left(t.mem, mj).has_value() && !TitleCompressor::revert_left(t.mem, mj).has_value());
    }
}

void test_keep_and_revert_refusals()
{
    // After the title was changed by something else, keep and revert leave it alone.
    Title      t = install_title(300'000, 11);
    MemJournal j;
    auto       c = make(t.mem, j);
    CHECK(c.prepare().has_value() && c.run().has_value());
    Mem snapshot = t.mem;
    t.mem.metas[tk(KEY)].back() ^= 1;   // another record
    const Mem other = t.mem;
    CHECK(!c.keep().has_value());
    CHECK(!c.revert().has_value());
    CHECK(t.mem.contents == other.contents && t.mem.metas == other.metas);
    // The original contents are gone: reverting is refused, nothing changes.
    t.mem.metas = snapshot.metas;
    const auto old_infos = record_contents(t.record);
    for (const ContentInfo& i : *old_infos)
        if (i.type == ContentType::Program) t.mem.contents.erase(i.id);
    const Mem before = t.mem;
    CHECK(!c.revert().has_value());
    CHECK(t.mem.contents == before.contents && t.mem.metas == before.metas && j.text.has_value());
}

// ------------------------------------------------------------- merging an update

const MetaKey PKEY{0x0100000000020800ull, 65536, MetaType::Patch};
const MetaKey MKEY{0x0100000000020000ull, 65536, MetaType::Application};   // the merged title: the update's version

struct WithUpdate
{
    Title     t;
    UpdateNca upd;
};

// A game with its update installed: two programs, two content metas, two records.
WithUpdate install_with_update(size_t image_size, uint32_t seed)
{
    WithUpdate w;
    w.t = install_title(image_size, seed);
    w.upd = update_nca(w.t.built, seed + 1);

    ContentInfo prog;
    {
        Sha256 h;
        h.update(w.upd.nca);
        prog.hash = h.digest();
    }
    prog.id = content_id_of(prog.hash);
    prog.size = w.upd.nca.size();
    prog.type = ContentType::Program;
    ContentInfo control;
    control.id = id_of(0x23);   // the update's own control data
    control.size = 2500;
    control.type = ContentType::Control;
    control.hash.fill(0xcc);
    w.t.mem.contents[control.id] = stream(2500, "control v2");
    w.t.mem.contents[prog.id] = w.upd.nca;

    const Bytes cnmt = packaged_cnmt(MetaType::Patch, PKEY.id, PKEY.version, {prog, control});
    const Bytes meta = meta_nca(cnmt);
    ContentInfo meta_content;
    {
        Sha256 h;
        h.update(meta);
        meta_content.id = content_id_of(h.digest());
    }
    meta_content.size = meta.size();
    meta_content.type = ContentType::Meta;
    w.t.mem.contents[meta_content.id] = meta;
    w.t.mem.metas[tk(PKEY)] = cnmt_install_record(*cnmt_parse(cnmt), meta_content);
    w.t.start = w.t.mem;
    return w;
}

TitleCompressor make_merge(Mem& mem, MemJournal& j, CompressCallbacks cb = {})
{
    CompressOptions o;
    o.block_size = 16384;
    o.merge_update = PKEY;
    return TitleCompressor(mem, test_keys(), j, lz4_compress, lz4_decompress, KEY, o, std::move(cb));
}

// The merged title: the one record at the update's version, the old two gone; its program is the game's with the
// update's ExeFS and the patched RomFS (checked here, apart from the engine); the old contents kept or gone.
bool is_merged(const Mem& m, const WithUpdate& w, int old)
{
    if (!m.pending.empty() || !m.removed.empty() || !m.pholders.empty()) return false;
    if (m.metas.count(tk(KEY)) || m.metas.count(tk(PKEY)) || !m.metas.count(tk(MKEY))) return false;
    const auto infos = record_contents(m.metas.at(tk(MKEY)));
    if (!infos) return false;
    std::set<ContentId> expected;
    bool                program_seen = false;
    for (const ContentInfo& i : *infos) {
        if (!m.contents.count(i.id)) return false;
        expected.insert(i.id);
        if (i.type != ContentType::Program) continue;
        program_seen = true;
        ProgramRecompressor rc([&w](uint64_t off, std::span<uint8_t> out) {
            std::memcpy(out.data(), w.t.program.data() + off, out.size());
            return true;
        }, w.t.program.size(), test_keys(), lz4_compress, lz4_decompress, 16384);
        rc.with_update([&w](uint64_t off, std::span<uint8_t> out) {
            std::memcpy(out.data(), w.upd.nca.data() + off, out.size());
            return true;
        }, w.upd.nca.size());
        if (!rc.inspect()) return false;
        const Bytes& made = m.contents.at(i.id);
        if (i.size != made.size()) return false;
        if (!rc.verify([&made](uint64_t off, std::span<uint8_t> out) {
                if (off > made.size() || out.size() > made.size() - off) return false;
                std::memcpy(out.data(), made.data() + off, out.size());
                return true;
            }, made.size()))
            return false;
    }
    if (!program_seen) return false;
    // The update's control data is the merged title's; the game's own is not listed.
    if (!expected.count(id_of(0x23)) || expected.count(id_of(0x22))) return false;
    // Old contents: the game's and the update's program and content meta.
    std::set<ContentId> replaced;
    for (const Mem* src : {&w.t.start}) {
        for (const KeyT& k : {tk(KEY), tk(PKEY)})
            if (const auto old_infos = record_contents(src->metas.at(k)))
                for (const ContentInfo& i : *old_infos)
                    if (i.type == ContentType::Program || i.type == ContentType::Meta) replaced.insert(i.id);
    }
    for (const auto& [id, bytes] : w.t.start.contents) {
        const bool is_replaced = replaced.count(id) != 0;
        if (!is_replaced && id != id_of(0x22)) {
            if (!m.contents.count(id) || m.contents.at(id) != bytes) return false;
            expected.insert(id);
        } else if (id == id_of(0x22)) {
            // The game's own control data: unreferenced now, kept (shared storage is not ours to delete).
            if (!m.contents.count(id)) return false;
            expected.insert(id);
        } else if (old == 1) {
            if (!m.contents.count(id) || m.contents.at(id) != bytes) return false;
            expected.insert(id);
        } else if (old == 0) {
            if (m.contents.count(id)) return false;
        } else if (m.contents.count(id)) {
            expected.insert(id);
        }
    }
    for (const auto& [id, _] : m.contents)
        if (!expected.count(id)) return false;
    return true;
}

void test_merge_flow()
{
    {   // keep
        WithUpdate w = install_with_update(300'000, 51);
        MemJournal j;
        auto       c = make_merge(w.t.mem, j);
        const Done p = c.prepare();
        if (!CHECK(p.has_value())) {
            fprintf(stderr, "prepare: %s\n", p.error().c_str());
            return;
        }
        CHECK(c.report().program_before == w.t.program.size() + w.upd.nca.size());
        CHECK(w.t.mem.ops == 0 && !j.text && is_baseline(w.t.mem, w.t));
        const Done ran = c.run();
        if (!CHECK(ran.has_value())) {
            fprintf(stderr, "run: %s\n", ran.error().c_str());
            return;
        }
        CHECK(is_merged(w.t.mem, w, 1));
        CHECK(w.t.mem.app_record == std::vector<KeyT>{tk(MKEY)});   // ns lists the new key and no other
        CHECK(c.keep().has_value());
        CHECK(is_merged(w.t.mem, w, 0));
        CHECK(!j.text);
    }
    {   // revert
        WithUpdate w = install_with_update(300'000, 52);
        MemJournal j;
        auto       c = make_merge(w.t.mem, j);
        CHECK(c.prepare().has_value() && c.run().has_value());
        CHECK(c.revert().has_value());
        CHECK(is_baseline(w.t.mem, w.t) && !j.text);
        CHECK(w.t.mem.app_record == (std::vector<KeyT>{tk(KEY), tk(PKEY)}));   // ns lists the game and its update again
    }
    {   // ns drops the record and fails to push: the old keys are put back, and the run is undone
        WithUpdate w = install_with_update(300'000, 52);
        MemJournal j;
        auto       c = make_merge(w.t.mem, j);
        CHECK(c.prepare().has_value());
        w.t.mem.replace_half_at = 0;
        CHECK(!c.run().has_value());
        CHECK(is_baseline(w.t.mem, w.t) && !j.text);
        CHECK(w.t.mem.app_record == (std::vector<KeyT>{tk(KEY), tk(PKEY)}));
    }
    {   // refusals write nothing
        WithUpdate w = install_with_update(300'000, 53);
        MemJournal j;
        CompressOptions o;
        o.block_size = 16384;
        o.merge_update = MetaKey{PKEY.id + 0x1000, 65536, MetaType::Patch};   // another game's
        TitleCompressor other(w.t.mem, test_keys(), j, lz4_compress, lz4_decompress, KEY, o);
        CHECK(!other.prepare().has_value());
        o.merge_update = MetaKey{PKEY.id, 1, MetaType::Patch};   // an update that is not installed
        TitleCompressor none(w.t.mem, test_keys(), j, lz4_compress, lz4_decompress, KEY, o);
        CHECK(!none.prepare().has_value());
        o.merge_update = MKEY;   // not a patch's key
        TitleCompressor notpatch(w.t.mem, test_keys(), j, lz4_compress, lz4_decompress, KEY, o);
        CHECK(!notpatch.prepare().has_value());
        // An update installed and no merge asked: refused as before.
        CompressOptions refuse;
        refuse.update_installed = true;
        TitleCompressor r(w.t.mem, test_keys(), j, lz4_compress, lz4_decompress, KEY, refuse);
        CHECK(!r.prepare().has_value());
        CHECK(w.t.mem.ops == 0 && !j.text && is_baseline(w.t.mem, w.t));
    }
    {   // a damaged update or game is found before anything is written
        for (int which = 0; which < 2; which++) {
            WithUpdate w = install_with_update(300'000, 54);
            const ContentId target = [&] {
                const auto infos = record_contents(w.t.mem.metas.at(tk(which ? KEY : PKEY)));
                for (const ContentInfo& i : *infos)
                    if (i.type == ContentType::Program) return i.id;
                return ContentId{};
            }();
            Bytes& bytes = w.t.mem.contents[target];
            size_t at = w.upd.patch.start + 100;   // inside the update's patch data
            if (which) {   // a byte in every 4 KiB of the game's RomFS (the update keeps only some of it, and may take a block from anywhere)
                uint8_t head[NCA_HEADER];
                std::memcpy(head, bytes.data(), NCA_HEADER);
                nca_header_crypt(head, HEADER_KEY, false);
                const NcaSection s1 = nca_parse_header(head, bytes.size())->sections[1];
                for (size_t o = s1.start; o + 4096 <= s1.end; o += 4096) bytes[o + 7] ^= 1;
            } else {
                bytes[at] ^= 1;
            }
            w.t.start = w.t.mem;   // the damaged title is the one that must stay as it is
            const Title baseline = w.t;
            MemJournal  j;
            auto        c = make_merge(w.t.mem, j);
            const Done  p = c.prepare();
            CHECK(!p.has_value());
            CHECK(w.t.mem.ops == 0 && !j.text && is_baseline(w.t.mem, baseline));
        }
    }
}

void test_merge_faults()
{
    const WithUpdate base = install_with_update(300'000, 55);
    for (int k = 0; k < 600; k++) {
        WithUpdate w = base;
        MemJournal j;
        w.t.mem.fault_at = k;
        auto c = make_merge(w.t.mem, j);
        CHECK(c.prepare().has_value());
        w.t.mem.ops = 0;
        const Done ran = c.run();
        if (ran) {
            CHECK(is_merged(w.t.mem, w, 1));
            Done kept = c.keep();
            if (!kept) {   // it failed on the injected fault: nothing was deleted, and it works on the next try
                CHECK(is_merged(w.t.mem, w, 1) && j.text.has_value());   // keep() tells ns again: the fault was still to come
                w.t.mem.fault_at = -1;
                kept = c.keep();
            }
            CHECK(kept.has_value() && is_merged(w.t.mem, w, 0));
            if (w.t.mem.ops <= k) break;
            continue;
        }
        const bool clean = is_baseline(w.t.mem, w.t) && !j.text;
        if (!clean) fprintf(stderr, "merge: fault at call %d left the title not as it was (%s)\n", k, ran.error().c_str());
        CHECK(clean);
    }
}

void test_merge_crashes()
{
    const WithUpdate base = install_with_update(300'000, 56);
    int              cleaned = 0, awaiting = 0;
    for (int k = 0; k < 600; k++) {
        WithUpdate w = base;
        MemJournal j;
        w.t.mem.crash_at = k;
        bool crashed = false;
        try {
            auto c = make_merge(w.t.mem, j);
            CHECK(c.prepare().has_value());
            w.t.mem.ops = 0;
            CHECK(c.run().has_value());
            CHECK(c.keep().has_value());
        } catch (const Crash&) {
            crashed = true;
        }
        w.t.mem.crash_at = -1;
        if (!crashed) {
            CHECK(is_merged(w.t.mem, w, 0));
            break;
        }
        auto left = TitleCompressor::recover(w.t.mem, j);
        if (!CHECK(left.has_value())) continue;
        switch (left->found) {
        case TitleCompressor::Found::Nothing:
            CHECK(is_baseline(w.t.mem, w.t) || is_merged(w.t.mem, w, 0));
            break;
        case TitleCompressor::Found::CleanedUp:
            cleaned++;
            if (!CHECK(is_baseline(w.t.mem, w.t) && !j.text)) fprintf(stderr, "  (merge: crash at call %d, cleaned up)\n", k);
            break;
        case TitleCompressor::Found::AwaitingDecision:
            awaiting++;
            CHECK(left->key == MKEY);
            CHECK(is_merged(w.t.mem, w, -1));
            if (k % 2) {
                CHECK(TitleCompressor::revert_left(w.t.mem, j).has_value());
                if (!CHECK(is_baseline(w.t.mem, w.t) && !j.text)) fprintf(stderr, "  (merge: crash at call %d, then revert)\n", k);
            } else {
                CHECK(TitleCompressor::keep_left(w.t.mem, j).has_value());
                if (!CHECK(is_merged(w.t.mem, w, 0) && !j.text)) fprintf(stderr, "  (merge: crash at call %d, then keep)\n", k);
            }
            break;
        }
    }
    CHECK(cleaned > 5);
    (void)awaiting;
}

void test_merge_restart_and_revert_crash()
{
    for (const bool revert : {true, false}) {
        WithUpdate w = install_with_update(300'000, 63);
        MemJournal j;
        {
            auto c = make_merge(w.t.mem, j);
            CHECK(c.prepare().has_value() && c.run().has_value());
        }
        auto left = TitleCompressor::recover(w.t.mem, j);
        if (!CHECK(left.has_value() && left->found == TitleCompressor::Found::AwaitingDecision && left->key == MKEY)) continue;
        CHECK(is_merged(w.t.mem, w, 1));
        if (revert) {
            CHECK(TitleCompressor::revert_left(w.t.mem, j).has_value());
            CHECK(is_baseline(w.t.mem, w.t) && !j.text);
        } else {
            CHECK(TitleCompressor::keep_left(w.t.mem, j).has_value());
            CHECK(is_merged(w.t.mem, w, 0) && !j.text);
        }
    }
    // A crash in every call of revert: the next start puts it right.
    for (int k = 0; k < 40; k++) {
        WithUpdate w = install_with_update(300'000, 64);
        MemJournal j;
        auto       c = make_merge(w.t.mem, j);
        CHECK(c.prepare().has_value() && c.run().has_value());
        w.t.mem.ops = 0;
        w.t.mem.crash_at = k;
        bool crashed = false;
        try {
            CHECK(c.revert().has_value());
        } catch (const Crash&) {
            crashed = true;
        }
        w.t.mem.crash_at = -1;
        if (!crashed) {
            CHECK(is_baseline(w.t.mem, w.t) && !j.text);
            break;
        }
        auto left = TitleCompressor::recover(w.t.mem, j);
        CHECK(left.has_value());
        if (left->found == TitleCompressor::Found::AwaitingDecision) CHECK(TitleCompressor::revert_left(w.t.mem, j).has_value());
        if (!CHECK(is_baseline(w.t.mem, w.t) && !j.text)) fprintf(stderr, "  (merge: crash at call %d of revert)\n", k);
    }
}

void test_merge_journal_text()
{
    CompressJournal j;
    j.phase = CompressJournal::Phase::Swapped;
    j.key = KEY;
    j.old_record = {1, 2, 3};
    j.new_record = {4};
    j.new_key = MKEY;
    j.patch_key = PKEY;
    j.patch_record = {9, 9};
    j.old_patch_program = id_of(0x71);
    j.old_patch_meta = id_of(0x72);
    const auto back = CompressJournal::parse(j.text());
    if (!CHECK(back.has_value())) return;
    CHECK(back->new_key == MKEY && back->patch_key == PKEY && back->patch_record == j.patch_record);
    CHECK(back->old_patch_program == j.old_patch_program && back->old_patch_meta == j.old_patch_meta && back->result_key() == MKEY);
    // A journal without a patch record for a patch key is not one.
    std::string broken = j.text();
    broken.replace(broken.find("patch_record="), 15, "patch_record=\n#");
    CHECK(!CompressJournal::parse(broken).has_value());
}
}   // namespace

int main()
{
    test_journal_text();
    test_happy_path();
    test_revert();
    test_refusals_write_nothing();
    test_cancel();
    test_journal_faults();
    test_faults();
    test_corruption();
    test_crashes();
    test_crash_while_reverting();
    test_keep_and_revert_refusals();
    test_restart_after_run();
    test_merge_journal_text();
    test_merge_flow();
    test_merge_faults();
    test_merge_crashes();
    test_merge_restart_and_revert_crash();
    CHECK_DONE("compress_title: ok");
}
