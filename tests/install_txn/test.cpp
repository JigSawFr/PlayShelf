// install/package.cpp and install/installer.cpp against a console in
// memory: an NSP, an NSZ and an XCI built here installed whole (contents
// registered byte for byte, the content meta record ncm keeps, the
// application record, the ticket); what prepare() refuses before writing
// anything (a missing content, no room, a newer system, a ticket without its
// certificate); and a failure or a cancel at every step of run() leaving the
// console as it was, a content meta record it replaced put back.
// With PLAYSHELF_WRITE_SEEDS=<dir>, the packages are written there as the
// fuzzer's seeds (tests/fuzz/install_package/corpus).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>

#include "../install/packages.hpp"
#include "check.h"
#include "install/installer.hpp"
#include "install/package.hpp"

using namespace install;
using namespace test_install;

namespace
{
void write_seed(const std::string& name, const Bytes& b)
{
    const char* dir = std::getenv("PLAYSHELF_WRITE_SEEDS");
    if (!dir) return;
    std::ofstream(std::string(dir) + "/" + name, std::ios::binary).write((const char*)b.data(), (long)b.size());
}

// ---------------------------------------------------------------- the console

struct FakeConsole : Console
{
    std::map<ContentId, Bytes>        placeholders, contents;
    std::map<std::string, Bytes>      meta;   // committed
    std::map<std::string, Bytes>      meta_pending;
    std::set<std::string>             meta_removed;
    std::vector<std::pair<uint64_t, MetaKey>> app_records;
    std::vector<std::pair<Bytes, Bytes>>      tickets;
    uint64_t                          space = 1u << 30;
    uint32_t                          version = 0x0c0000;   // 12.0.0
    // A failure injected at the n-th call of `fail_at` (0: none).
    std::string fail_at;
    int         fail_n = 0, calls = 0;

    static std::string key(const MetaKey& k) { return std::to_string(k.id) + ":" + std::to_string(k.version); }
    bool               inject(const char* what, std::string* why)
    {
        if (fail_at == what && ++calls == fail_n) {
            *why = std::string("injected: ") + what;
            return true;
        }
        return false;
    }
    bool has_content(const ContentId& id) override { return contents.count(id) != 0; }
    bool create_placeholder(const ContentId& id, uint64_t size, std::string* why) override
    {
        if (inject("create", why)) return false;
        if (placeholders.count(id)) {
            *why = "placeholder exists";
            return false;
        }
        placeholders[id].reserve(size);
        return true;
    }
    bool write_placeholder(const ContentId& id, uint64_t offset, const uint8_t* data, size_t size, std::string* why) override
    {
        if (inject("write", why)) return false;
        Bytes& p = placeholders.at(id);
        if (offset != p.size()) {
            *why = "write out of order";
            return false;
        }
        p.insert(p.end(), data, data + size);
        return true;
    }
    bool register_placeholder(const ContentId& id, std::string* why) override
    {
        if (inject("register", why)) return false;
        contents[id] = placeholders.at(id);
        return true;
    }
    void     delete_placeholder(const ContentId& id) override { placeholders.erase(id); }
    void     delete_content(const ContentId& id) override { contents.erase(id); }
    uint64_t free_space() override { return space; }
    std::optional<Bytes> get_meta(const MetaKey& k) override
    {
        const auto it = meta.find(key(k));
        if (it == meta.end()) return std::nullopt;
        return it->second;
    }
    bool set_meta(const MetaKey& k, const Bytes& record, std::string* why) override
    {
        if (inject("set_meta", why)) return false;
        meta_pending[key(k)] = record;
        meta_removed.erase(key(k));
        return true;
    }
    bool remove_meta(const MetaKey& k, std::string*) override
    {
        meta_pending.erase(key(k));
        meta_removed.insert(key(k));
        return true;
    }
    bool commit_meta(std::string* why) override
    {
        if (inject("commit", why)) return false;
        for (auto& kv : meta_pending)
            meta[kv.first] = kv.second;
        for (const auto& k : meta_removed)
            meta.erase(k);
        meta_pending.clear();
        meta_removed.clear();
        return true;
    }
    bool push_application_record(uint64_t app, const MetaKey& k, std::string* why) override
    {
        if (inject("record", why)) return false;
        app_records.push_back({app, k});
        return true;
    }
    bool import_ticket(const Bytes& t, const Bytes& c, std::string* why) override
    {
        if (inject("ticket", why)) return false;
        tickets.push_back({t, c});
        return true;
    }
    uint32_t system_version() override { return version; }
};

struct Run
{
    bool        prepared = false, ok = false;
    std::string why;
};

Run install(FakeConsole& console, const Bytes& package, InstallOptions options = {SignaturePolicy::Off, false},
            InstallCallbacks cb = {})
{
    Run          r;
    MemorySource src(package, 0x1000);
    const auto   pkg = package_open(src, &r.why);
    if (!pkg) return r;
    Installer inst(src, *pkg, console, test_keys(), options, std::move(cb));
    r.prepared = inst.prepare(&r.why);
    if (r.prepared) r.ok = inst.run(&r.why);
    return r;
}

void check_installed(FakeConsole& c, const Title& t)
{
    CHECK(c.contents.size() == 3);
    CHECK(c.contents[t.program_info.id] == t.program);
    CHECK(c.contents[t.control_info.id] == t.control);
    CHECK(c.contents[t.meta_id] == t.meta_nca);
    CHECK(c.placeholders.empty());
    const MetaKey key{0x0100000000010000ull, 0, MetaType::Application};
    const auto    rec = c.get_meta(key);
    if (CHECK(rec)) {
        const auto meta = cnmt_parse(t.cnmt.data(), t.cnmt.size());
        ContentInfo mc = info_of(t.meta_nca, ContentType::Meta, 0x44);
        CHECK(*rec == cnmt_install_record(*meta, mc));
    }
    CHECK(c.app_records.size() == 1 && c.app_records[0].first == 0x0100000000010000ull);
    CHECK(c.tickets.size() == 1 && c.tickets[0].first.size() == 0x2c0 && c.tickets[0].second.size() == 0x700);
}

// ---------------------------------------------------------------- tests

void test_packages()
{
    for (bool compressed : {false, true}) {
        const Title t = application(compressed);
        write_seed(compressed ? "nsz" : "nsp", nsp(t));
        write_seed(compressed ? "xcz" : "xci", xci(t));
        for (const Bytes& pkg : {nsp(t), xci(t)}) {
            FakeConsole c;
            uint64_t    last = 0, total = 0;
            InstallCallbacks cb;
            cb.progress = [&](uint64_t done, uint64_t all, const std::string&) {
                CHECK(done >= last);
                last = done;
                total = all;
            };
            const Run r = install(c, pkg, {SignaturePolicy::Off, false}, cb);
            CHECK(r.prepared && r.ok);
            if (!r.ok) fprintf(stderr, "  %s\n", r.why.c_str());
            check_installed(c, t);
            CHECK(total == t.program.size() + t.control.size() + t.meta_nca.size() && last == total);
            // Again: everything is there, nothing to write; the record set anew.
            const Run again = install(c, pkg);
            CHECK(again.ok);
            CHECK(c.contents.size() == 3 && c.app_records.size() == 2);
        }
    }
    // The package's kind.
    const Title  t = application();
    MemorySource a(nsp(t)), b(xci(t));
    CHECK(package_open(a)->kind == PackageKind::Nsp);
    CHECK(package_open(b)->kind == PackageKind::Xci);
    CHECK(package_open(b)->tickets().size() == 1);
    MemorySource junk(stream(0x400, "junk"));
    CHECK(!package_open(junk));
}

void test_prepare_refuses()
{
    const Title t = application();
    // A content the package lacks.
    Title missing = t;
    missing.files.erase(missing.files.begin() + 2);
    FakeConsole c1;
    Run         r = install(c1, nsp(missing));
    CHECK(!r.prepared && r.why.find("lacks") != std::string::npos);
    CHECK(c1.contents.empty() && c1.placeholders.empty());
    // ...unless the console has it already.
    FakeConsole c2;
    c2.contents[t.control_info.id] = t.control;
    CHECK(install(c2, nsp(missing)).ok);
    // No room.
    FakeConsole c3;
    c3.space = t.program.size();
    r = install(c3, nsp(t));
    CHECK(!r.prepared && r.why.find("space") != std::string::npos);
    // A newer system than the console's: refused, unless told to ignore it.
    const Title newer = application(false, (13u << 26));
    FakeConsole c4;
    r = install(c4, nsp(newer));
    CHECK(!r.prepared && r.why.find("13.0.0") != std::string::npos);
    FakeConsole c5;
    r = install(c5, nsp(newer), {SignaturePolicy::Off, true});
    CHECK(r.ok);
    const auto rec = c5.get_meta(MetaKey{0x0100000000010000ull, 0, MetaType::Application});
    CHECK(rec && le32(rec->data() + 8 + 8) == 0);
    // A ticket without its certificate.
    Title no_cert = t;
    no_cert.files.pop_back();
    FakeConsole c6;
    r = install(c6, nsp(no_cert));
    CHECK(!r.prepared && r.why.find("certificate") != std::string::npos);
    // No content meta at all.
    FakeConsole c7;
    r = install(c7, archive(PfsKind::Pfs0, {t.files[1]}));
    CHECK(!r.prepared);
}

void test_signatures()
{
    const Title t = application();
    FakeConsole c1;
    Run         r = install(c1, nsp(t), {SignaturePolicy::Require, false});
    CHECK(!r.ok && r.why.find("not signed by Nintendo") != std::string::npos);
    CHECK(c1.contents.empty() && c1.placeholders.empty());
    // Asked: once yes, the rest of the package is taken.
    int              asked = 0;
    InstallCallbacks yes;
    yes.accept_signature = [&](const std::string&) { return ++asked > 0; };
    FakeConsole c2;
    CHECK(install(c2, nsp(t), {SignaturePolicy::Ask, false}, yes).ok);
    CHECK(asked == 1);
    InstallCallbacks no;
    no.accept_signature = [](const std::string&) { return false; };
    FakeConsole c3;
    CHECK(!install(c3, nsp(t), {SignaturePolicy::Ask, false}, no).ok);
    CHECK(c3.contents.empty());
}

void test_undo()
{
    const Title   t = application(true);
    const MetaKey key{0x0100000000010000ull, 0, MetaType::Application};
    const Bytes   before = stream(0x40, "an older record");
    // A failure at each call of each step: the console as it was.
    for (const char* step : {"create", "write", "register", "ticket", "set_meta", "commit", "record"})
        for (int n = 1; n <= 4; n++) {
            FakeConsole c;
            c.meta[FakeConsole::key(key)] = before;
            c.fail_at = step;
            c.fail_n = n;
            const Run r = install(c, nsp(t));
            if (c.calls < n) {   // the step happens fewer times than that
                CHECK(r.ok);
                continue;
            }
            CHECK(r.prepared && !r.ok);
            CHECK(r.why.find("injected") != std::string::npos);
            CHECK(c.contents.empty());
            CHECK(c.placeholders.empty());
            CHECK(c.meta.size() == 1 && c.meta[FakeConsole::key(key)] == before);
        }
    // Cancelled from the progress callback, midway.
    FakeConsole       c;
    std::atomic<bool> stop{false};
    InstallCallbacks  cb;
    cb.cancel = &stop;
    cb.progress = [&](uint64_t done, uint64_t, const std::string&) {
        if (done > 0x6000) stop = true;
    };
    const Run r = install(c, nsp(t), {SignaturePolicy::Off, false}, cb);
    CHECK(!r.ok && r.why == "cancelled");
    CHECK(c.contents.empty() && c.placeholders.empty() && c.meta.empty());
    // A broken content (a byte changed): its hash says so, the rest undone.
    Title broken = application();
    broken.files[2].data[0x4100] ^= 1;
    FakeConsole cb2;
    const Run   rb = install(cb2, nsp(broken));
    CHECK(!rb.ok && rb.why.find("SHA-256") != std::string::npos);
    CHECK(cb2.contents.empty() && cb2.placeholders.empty());
}
}   // namespace

int main()
{
    test_packages();
    test_prepare_refuses();
    test_signatures();
    test_undo();
    CHECK_DONE("install transaction: NSP, NSZ and XCI installed; refusals before writing; every failure undone passed");
}
