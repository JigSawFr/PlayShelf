// install/compress_title.hpp: an installed game made over, as a transaction.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/compress_title.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>

#include "install/recompress_meta.hpp"
#include "util/sha256.hpp"

namespace install
{

namespace
{
constexpr uint64_t MAX_META_NCA = 4u << 20;
constexpr size_t   PIECE = 1u << 20;

std::string hex_of(std::span<const uint8_t> b)
{
    static const char H[] = "0123456789abcdef";
    std::string       s;
    for (uint8_t x : b) {
        s += H[x >> 4];
        s += H[x & 15];
    }
    return s;
}

std::optional<std::vector<uint8_t>> bytes_of(const std::string& hex)
{
    if (hex.size() % 2) return std::nullopt;
    std::vector<uint8_t> out;
    auto                 v = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
    for (size_t i = 0; i < hex.size(); i += 2) {
        const int a = v(hex[i]), b = v(hex[i + 1]);
        if (a < 0 || b < 0) return std::nullopt;
        out.push_back((uint8_t)(a << 4 | b));
    }
    return out;
}

std::array<uint8_t, 32> sha(std::span<const uint8_t> b)
{
    Sha256 h;
    h.update(b);
    return h.digest();
}

// A placeholder id only this module uses, from the content it replaces.
ContentId placeholder_for(const ContentId& of, char what)
{
    Sha256 h;
    h.update(std::string_view("playshelf-recompress/"));
    h.update(std::string_view(&what, 1));
    h.update(of);
    return content_id_of(h.digest());
}

bool in_record(const std::vector<uint8_t>& record, const ContentId& id)
{
    const auto infos = record_contents(record);
    if (!infos) return true;   // unreadable: assume it is used
    for (const ContentInfo& c : *infos)
        if (c.id == id) return true;
    return false;
}

// What this run made and nothing uses: deleted.
void delete_ours(Console& c, const CompressJournal& j)
{
    const auto current = c.get_meta(j.key);
    for (const ContentId& p : j.placeholders)
        if (c.has_placeholder(p)) c.delete_placeholder(p);
    for (const ContentId& id : j.contents) {
        if (current && in_record(*current, id)) continue;
        if (in_record(j.old_record, id)) continue;
        if (c.has_content(id)) c.delete_content(id);
    }
}

// Reads a content in windows, so the many small reads of a check are few calls.
struct Cached
{
    Console&             console;
    ContentId            id;
    std::vector<uint8_t> win;
    uint64_t             at = ~0ull;
    uint64_t             size = 0;

    bool read(uint64_t off, std::span<uint8_t> out)
    {
        if (off > size || out.size() > size - off) return false;
        if (out.size() >= PIECE) return console.read_content(id, off, out).has_value();
        if (at == ~0ull || off < at || off + out.size() > at + win.size()) {
            const uint64_t n = std::min<uint64_t>(PIECE, size - off);
            win.resize((size_t)n);
            if (!console.read_content(id, off, win)) {
                at = ~0ull;
                return false;
            }
            at = off;
        }
        std::memcpy(out.data(), win.data() + (off - at), out.size());
        return true;
    }
};

Done restore(Console& c, const CompressJournal& j)
{
    // The old record back, only if what it lists is still there.
    const auto infos = record_contents(j.old_record);
    if (!infos) return fail("the original record cannot be read");
    for (const ContentInfo& i : *infos)
        if (!c.has_content(i.id)) return fail("the original contents are gone: the old record cannot be put back");
    if (auto d = c.set_meta(j.key, j.old_record); !d) return d;
    if (auto d = c.commit_meta(); !d) return d;
    const auto back = c.get_meta(j.key);
    if (!back || *back != j.old_record) return fail("the original record did not come back");
    return {};
}

Done do_revert(Console& c, JournalStore& store, const CompressJournal& j)
{
    const auto cur = c.get_meta(j.key);
    if (cur && *cur == j.old_record) {
        delete_ours(c, j);
        store.clear();
        return {};
    }
    if (!cur || j.new_record.empty() || *cur != j.new_record) return fail("the title changed since: left as it is");
    if (auto d = restore(c, j); !d) return d;
    delete_ours(c, j);
    store.clear();
    return {};
}

Done do_keep(Console& c, JournalStore& store, const CompressJournal& j)
{
    const auto cur = c.get_meta(j.key);
    if (!cur || j.new_record.empty() || *cur != j.new_record) return fail("the title is not the compressed one: nothing to keep");
    for (const ContentId& id : j.contents)
        if (!c.has_content(id)) return fail("the new contents are missing: the old ones are kept");
    for (const ContentId& id : {j.old_program, j.old_meta})
        if (!in_record(*cur, id) && c.has_content(id)) c.delete_content(id);
    store.clear();
    return {};
}
}   // namespace

// ---------------------------------------------------------------- journal

std::string CompressJournal::text() const
{
    char b[40];
    std::snprintf(b, sizeof b, "%016llx:%u:%02x", (unsigned long long)key.id, (unsigned)key.version, (unsigned)std::to_underlying(key.type));
    std::ostringstream out;
    out << "schema=1\n";
    out << "phase=" << (phase == Phase::Writing ? "writing" : "swapped") << "\n";
    out << "key=" << b << "\n";
    out << "old_record=" << hex_of(old_record) << "\n";
    out << "new_record=" << hex_of(new_record) << "\n";
    out << "old_program=" << hex_of(old_program) << "\n";
    out << "old_meta=" << hex_of(old_meta) << "\n";
    for (const ContentId& p : placeholders) out << "placeholder=" << hex_of(p) << "\n";
    for (const ContentId& c : contents) out << "content=" << hex_of(c) << "\n";
    return out.str();
}

std::optional<CompressJournal> CompressJournal::parse(const std::string& text)
{
    CompressJournal j;
    bool            schema_ok = false, phase_ok = false, key_ok = false, old_ok = false;
    std::istringstream in(text);
    std::string        line;
    auto id_of_hex = [](const std::string& v) -> std::optional<ContentId> {
        const auto b = bytes_of(v);
        if (!b || b->size() != 16) return std::nullopt;
        ContentId id;
        std::memcpy(id.data(), b->data(), 16);
        return id;
    };
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "schema") {
            if (v != "1") return std::nullopt;   // a newer one is not ours to read (or rewrite)
            schema_ok = true;
        } else if (k == "phase") {
            if (v == "writing") j.phase = Phase::Writing;
            else if (v == "swapped") j.phase = Phase::Swapped;
            else return std::nullopt;
            phase_ok = true;
        } else if (k == "key") {
            unsigned long long id;
            unsigned           version, type;
            if (std::sscanf(v.c_str(), "%16llx:%u:%2x", &id, &version, &type) != 3) return std::nullopt;
            j.key = MetaKey{id, version, (MetaType)type};
            key_ok = true;
        } else if (k == "old_record") {
            const auto b = bytes_of(v);
            if (!b || b->empty()) return std::nullopt;
            j.old_record = *b;
            old_ok = true;
        } else if (k == "new_record") {
            const auto b = bytes_of(v);
            if (!b) return std::nullopt;
            j.new_record = *b;
        } else if (k == "old_program" || k == "old_meta") {
            const auto id = id_of_hex(v);
            if (!id) return std::nullopt;
            (k == "old_program" ? j.old_program : j.old_meta) = *id;
        } else if (k == "placeholder" || k == "content") {
            const auto id = id_of_hex(v);
            if (!id) return std::nullopt;
            (k == "placeholder" ? j.placeholders : j.contents).push_back(*id);
        }
    }
    if (!schema_ok || !phase_ok || !key_ok || !old_ok) return std::nullopt;
    return j;
}

// ------------------------------------------------------------ compressor

struct TitleCompressor::Impl
{
    Console&             console;
    NcaKeys              keys;
    JournalStore&        store;
    cstore::CompressFn   compress;
    cstore::DecompressFn decompress;
    MetaKey              key;
    CompressOptions      options;
    CompressCallbacks    cb;

    // What prepare() found.
    bool                                 prepared = false;
    std::vector<uint8_t>                 record;
    ContentInfo                          program, meta;   // as the record lists them
    std::vector<uint8_t>                 cnmt, meta_nca;
    std::unique_ptr<ProgramRecompressor> rc;
    std::vector<uint8_t>                 header;   // the new program's first bytes
    uint64_t                             new_size = 0, body_size = 0, rep_needed = 0;
    std::array<uint8_t, 32>              body_digest{};

    // What run() made.
    std::optional<CompressJournal> j;
    bool                           swapped = false;

    Impl(Console& c, NcaKeys k, JournalStore& s, cstore::CompressFn cf, cstore::DecompressFn df, MetaKey mk, CompressOptions o,
         CompressCallbacks callbacks)
        : console(c), keys(std::move(k)), store(s), compress(std::move(cf)), decompress(std::move(df)), key(mk), options(o),
          cb(std::move(callbacks))
    {
    }

    bool cancelled() const { return cb.cancel && cb.cancel->load(); }
    void progress(CompressStage s, uint64_t d, uint64_t t) const
    {
        if (cb.progress) cb.progress(s, d, t);
    }
    // Writes a whole content under the id its hash gives; the placeholder is
    // the module's own.
    Done write_new(const ContentId& placeholder, std::span<const uint8_t> bytes, ContentId* id_out);
};

TitleCompressor::TitleCompressor(Console& c, NcaKeys k, JournalStore& s, cstore::CompressFn cf, cstore::DecompressFn df,
                                 MetaKey mk, CompressOptions o, CompressCallbacks callbacks)
    : impl(std::make_unique<Impl>(c, std::move(k), s, std::move(cf), std::move(df), mk, o, std::move(callbacks)))
{
}

TitleCompressor::~TitleCompressor() = default;

Done TitleCompressor::prepare()
{
    Impl& m = *impl;
    Console& c = m.console;
    if (m.options.update_installed) return fail("an update is installed: it would not install over the compressed game");
    if (m.key.type != MetaType::Application) return fail("only a base game is compressed");
    if (c.system_version() < m.options.min_system_version) return fail("the firmware is older than 12.0.0: it does not read compressed games");

    const auto rec = c.get_meta(m.key);
    if (!rec) return fail("the title is not installed here");
    m.record = *rec;
    const auto infos = record_contents(m.record);
    if (!infos) return fail(infos.error());
    int programs = 0, metas = 0;
    for (const ContentInfo& i : *infos) {
        if (i.type == ContentType::Program) {
            m.program = i;
            programs++;
        } else if (i.type == ContentType::Meta) {
            m.meta = i;
            metas++;
        }
    }
    if (programs != 1 || metas != 1) return fail("a title with other than one program and one content meta");

    // Its content meta, read back from the .cnmt.nca it was installed from.
    const auto meta_size = c.content_size(m.meta.id);
    if (!meta_size || *meta_size > MAX_META_NCA || *meta_size != m.meta.size) return fail("the content meta NCA is not what the record says");
    m.meta_nca.resize((size_t)*meta_size);
    if (auto d = c.read_content(m.meta.id, 0, m.meta_nca); !d) return fail("the content meta NCA could not be read: " + d.error());
    const auto cnmt = nca_read_cnmt(m.meta_nca, m.keys);
    if (!cnmt) return fail("the content meta: " + cnmt.error());
    m.cnmt = *cnmt;
    const auto parsed = cnmt_parse(m.cnmt);
    if (!parsed) return fail("the content meta: " + parsed.error());
    bool listed = false;
    for (const ContentInfo& i : parsed->contents) listed = listed || (i.id == m.program.id && i.type == ContentType::Program);
    if (!listed) return fail("the program is not in the title's content meta");

    // The program, made over once to see what it comes to.
    const auto prog_size = c.content_size(m.program.id);
    if (!prog_size || *prog_size != m.program.size) return fail("the program NCA is not what the record says");
    const ContentId prog_id = m.program.id;
    m.rc = std::make_unique<ProgramRecompressor>(
        [&c, prog_id](uint64_t off, std::span<uint8_t> out) { return c.read_content(prog_id, off, out).has_value(); }, *prog_size,
        m.keys, m.compress, m.decompress, m.options.block_size);
    if (auto d = m.rc->inspect(); !d) return fail("this title cannot be compressed: " + d.error());

    Sha256   body;
    uint64_t counted = 0;
    const Done measured = m.rc->build(
        [&](std::span<const uint8_t> b) {
            body.update(b);
            counted += b.size();
            return true;
        },
        [&](uint64_t d, uint64_t t) { m.progress(CompressStage::Measuring, d, t); }, m.cb.cancel);
    if (!measured) return fail(m.cancelled() ? "cancelled" : measured.error());
    m.header = m.rc->new_header();
    m.new_size = m.rc->new_size();
    m.body_size = counted;
    m.body_digest = body.digest();
    if (m.new_size != NCA_FULL_HEADER + counted) return fail("the measured size is not the size made");

    rep = {};
    rep.program_before = *prog_size;
    rep.program_after = m.new_size;
    rep.free_space = c.free_space();
    rep.needed = m.new_size + m.meta.size + m.options.free_margin;
    m.rep_needed = rep.needed;
    if (rep.free_space < rep.needed) return fail("not enough free space for the new copy beside the old one");
    if (rep.saving_percent() < m.options.min_saving_percent)
        return fail("the gain is too small to be worth giving up the official updates");
    if (m.cb.confirm && !m.cb.confirm(rep)) return fail("declined");
    m.prepared = true;
    return {};
}

Done TitleCompressor::Impl::write_new(const ContentId& placeholder, std::span<const uint8_t> bytes, ContentId* id_out)
{
    const ContentId id = content_id_of(sha(bytes));
    if (console.has_content(id)) return fail("a content with the id the new one would take is already there");
    if (console.has_placeholder(placeholder)) console.delete_placeholder(placeholder);   // a leftover of this module's own
    if (auto d = console.create_placeholder(placeholder, bytes.size()); !d) return d;
    j->placeholders.push_back(placeholder);
    if (!store.save(j->text())) return fail("the journal could not be kept");
    for (uint64_t at = 0; at < bytes.size(); at += PIECE) {
        const size_t n = (size_t)std::min<uint64_t>(PIECE, bytes.size() - at);
        if (auto d = console.write_placeholder(placeholder, at, bytes.data() + at, n); !d) return d;
    }
    j->contents.push_back(id);
    if (!store.save(j->text())) return fail("the journal could not be kept");
    if (auto d = console.register_placeholder_as(placeholder, id); !d) return d;
    *id_out = id;
    return {};
}

Done TitleCompressor::run()
{
    Impl&    m = *impl;
    Console& c = m.console;
    if (!m.prepared) return fail("prepare() first");
    if (m.j) return fail("already run");

    // Nothing changed since prepare() looked.
    if (const auto now_record = c.get_meta(m.key); !now_record || *now_record != m.record) return fail("the title changed since it was measured");
    if (c.free_space() < m.rep_needed) return fail("not enough free space for the new copy beside the old one");

    // The journal first: with it, whatever happens next can be undone.
    m.j.emplace();
    CompressJournal& j = *m.j;
    j.phase = CompressJournal::Phase::Writing;
    j.key = m.key;
    j.old_record = m.record;
    j.old_program = m.program.id;
    j.old_meta = m.meta.id;
    if (!m.store.save(j.text())) {
        m.j.reset();
        return fail("the journal could not be kept: nothing was written");
    }
    auto abandon = [&](const std::string& why) -> Done {
        delete_ours(c, j);
        m.store.clear();
        m.j.reset();
        return fail(why);
    };
    auto stopped = [&] { return m.cancelled(); };
    if (stopped()) return abandon("cancelled");

    // The program, the second time, into a placeholder.
    const ContentId ph = placeholder_for(m.program.id, 'p');
    if (c.has_placeholder(ph)) c.delete_placeholder(ph);   // a leftover of this module's own
    if (auto d = c.create_placeholder(ph, m.new_size); !d) return abandon("the new program could not be created: " + d.error());
    j.placeholders.push_back(ph);
    if (!m.store.save(j.text())) return abandon("the journal could not be kept");

    Sha256        whole, body;
    uint64_t      pos = NCA_FULL_HEADER, written = 0;
    std::string   write_error;
    whole.update(m.header);
    if (auto d = c.write_placeholder(ph, 0, m.header.data(), m.header.size()); !d) return abandon("the new program could not be written: " + d.error());
    const Done made = m.rc->build(
        [&](std::span<const uint8_t> b) {
            if (auto d = c.write_placeholder(ph, pos, b.data(), b.size()); !d) {
                write_error = d.error();
                return false;
            }
            whole.update(b);
            body.update(b);
            pos += b.size();
            written += b.size();
            return true;
        },
        [&](uint64_t d, uint64_t t) { m.progress(CompressStage::Writing, d, t); }, m.cb.cancel);
    if (!made) return abandon(stopped() ? "cancelled" : !write_error.empty() ? "the new program could not be written: " + write_error : made.error());
    // The same bytes as the measured pass, or something is not deterministic.
    if (m.rc->new_header() != m.header || m.rc->new_size() != m.new_size || written != m.body_size || body.digest() != m.body_digest)
        return abandon("the second pass did not give the first one's bytes");
    const auto nca_hash = whole.digest();
    const ContentId new_program = content_id_of(nca_hash);
    if (c.has_content(new_program)) return abandon("a content with the id the new program would take is already there");
    j.contents.push_back(new_program);
    if (!m.store.save(j.text())) return abandon("the journal could not be kept");
    if (auto d = c.register_placeholder_as(ph, new_program); !d) return abandon("the new program could not be registered: " + d.error());

    // Read back: the hash, then the whole of it against the original.
    {
        Cached rd{c, new_program, {}, ~0ull, m.new_size};
        Sha256 again;
        std::vector<uint8_t> buf(PIECE);
        for (uint64_t at = 0; at < m.new_size;) {
            if (stopped()) return abandon("cancelled");
            const size_t n = (size_t)std::min<uint64_t>(PIECE, m.new_size - at);
            if (!rd.read(at, {buf.data(), n})) return abandon("the new program could not be read back");
            again.update({buf.data(), n});
            at += n;
            m.progress(CompressStage::Checking, at, m.new_size * 2);
        }
        if (again.digest() != nca_hash) return abandon("the new program does not read back as it was written");
        const Done same = m.rc->verify(
            [&](uint64_t off, std::span<uint8_t> out) { return rd.read(off, out); }, m.new_size,
            [&](uint64_t d, uint64_t t) { m.progress(CompressStage::Checking, m.new_size + std::min(d, t) * m.new_size / std::max<uint64_t>(t, 1), m.new_size * 2); },
            m.cb.cancel);
        if (!same) return abandon(stopped() ? "cancelled" : "the new program differs from the original: " + same.error());
    }

    // The content meta, with the program as it now is, in an NCA of its own.
    ContentInfo now;
    now.hash = nca_hash;
    now.id = new_program;
    now.size = m.new_size;
    now.type = ContentType::Program;
    const auto new_cnmt = cnmt_with_program(m.cnmt, m.program.id, now);
    if (!new_cnmt) return abandon("the new content meta: " + new_cnmt.error());
    const auto new_meta_nca = meta_nca_with_cnmt(m.meta_nca, *new_cnmt, m.keys);
    if (!new_meta_nca) return abandon("the new content meta NCA: " + new_meta_nca.error());
    ContentId new_meta{};
    if (auto d = m.write_new(placeholder_for(m.meta.id, 'm'), *new_meta_nca, &new_meta); !d) return abandon("the new content meta could not be written: " + d.error());
    {
        const auto back_size = c.content_size(new_meta);
        std::vector<uint8_t> back(new_meta_nca->size());
        if (!back_size || *back_size != back.size() || !c.read_content(new_meta, 0, back) || back != *new_meta_nca)
            return abandon("the new content meta does not read back as it was written");
        const auto cnmt_back = nca_read_cnmt(back, m.keys);
        if (!cnmt_back || *cnmt_back != *new_cnmt) return abandon("the new content meta NCA does not hold the new content meta");
    }

    // The record: the old one with the two contents replaced.
    const auto record = record_with_contents(m.record, m.key.type, {{m.program.id, new_program, m.new_size}, {m.meta.id, new_meta, new_meta_nca->size()}});
    if (!record) return abandon("the new record: " + record.error());
    if (stopped()) return abandon("cancelled");
    j.new_record = *record;
    if (!m.store.save(j.text())) return abandon("the journal could not be kept");

    // The swap. From here a failure puts the old record back.
    auto undo = [&](const std::string& why) -> Done {
        const Done back = restore(c, j);
        if (!back) return fail(why + " (and the original could not be restored: " + back.error() + "; the journal is kept)");
        delete_ours(c, j);
        m.store.clear();
        m.j.reset();
        return fail(why);
    };
    if (auto d = c.set_meta(m.key, *record); !d) return undo("the record could not be replaced: " + d.error());
    if (auto d = c.commit_meta(); !d) return undo("the record could not be committed: " + d.error());
    const auto after = c.get_meta(m.key);
    if (!after || *after != *record) return undo("the record does not read back as it was written");
    for (const ContentId& id : {new_program, new_meta, m.program.id, m.meta.id})
        if (!c.has_content(id)) return undo("a content is missing after the swap");
    j.phase = CompressJournal::Phase::Swapped;
    if (!m.store.save(j.text())) return undo("the journal could not be kept");
    m.swapped = true;
    return {};
}

Done TitleCompressor::keep()
{
    Impl& m = *impl;
    if (!m.swapped || !m.j) return fail("nothing to keep");
    const Done d = do_keep(m.console, m.store, *m.j);
    if (d) m.swapped = false;
    return d;
}

Done TitleCompressor::revert()
{
    Impl& m = *impl;
    if (!m.swapped || !m.j) return fail("nothing to revert");
    const Done d = do_revert(m.console, m.store, *m.j);
    if (d) m.swapped = false;
    return d;
}

Parsed<TitleCompressor::Left> TitleCompressor::recover(Console& c, JournalStore& store)
{
    const auto text = store.load();
    if (!text) return Left{};
    const auto j = CompressJournal::parse(*text);
    if (!j) return Left{};   // another version's: not read, not touched
    const auto cur = c.get_meta(j->key);
    if (cur && !j->new_record.empty() && *cur == j->new_record) {
        Left l;
        l.found = Found::AwaitingDecision;
        l.key = j->key;
        return l;
    }
    // The record is not ours (still the old one, or changed since): whatever
    // this run made and nothing uses goes; the title is not touched.
    delete_ours(c, *j);
    store.clear();
    Left l;
    l.found = Found::CleanedUp;
    return l;
}

Done TitleCompressor::keep_left(Console& c, JournalStore& store)
{
    const auto text = store.load();
    const auto j = text ? CompressJournal::parse(*text) : std::nullopt;
    if (!j) return fail("nothing to keep");
    return do_keep(c, store, *j);
}

Done TitleCompressor::revert_left(Console& c, JournalStore& store)
{
    const auto text = store.load();
    const auto j = text ? CompressJournal::parse(*text) : std::nullopt;
    if (!j) return fail("nothing to revert");
    return do_revert(c, store, *j);
}

}   // namespace install
