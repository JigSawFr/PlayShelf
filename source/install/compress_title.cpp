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

// Used by what the title was before this run (the game's record, the update's).
bool in_olds(const CompressJournal& j, const ContentId& id)
{
    return in_record(j.old_record, id) || (j.patch_key && in_record(j.patch_record, id));
}

// What this run made and nothing uses: deleted.
void delete_ours(Console& c, const CompressJournal& j)
{
    const auto current = c.get_meta(j.result_key());
    for (const ContentId& p : j.placeholders)
        if (c.has_placeholder(p)) c.delete_placeholder(p);
    for (const ContentId& id : j.contents) {
        if (current && in_record(*current, id)) continue;
        if (in_olds(j, id)) continue;
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

// The records the title had, with their keys.
std::vector<std::pair<MetaKey, const std::vector<uint8_t>*>> olds_of(const CompressJournal& j)
{
    std::vector<std::pair<MetaKey, const std::vector<uint8_t>*>> v = {{j.key, &j.old_record}};
    if (j.patch_key) v.push_back({*j.patch_key, &j.patch_record});
    return v;
}

bool is_old_key(const CompressJournal& j, const MetaKey& k)
{
    for (const auto& o : olds_of(j))
        if (o.first == k) return true;
    return false;
}

// The title as it was: every old record at its key, and the new key (when it is none of them) gone.
bool is_as_before(Console& c, const CompressJournal& j)
{
    for (const auto& o : olds_of(j)) {
        const auto r = c.get_meta(o.first);
        if (!r || *r != *o.second) return false;
    }
    return is_old_key(j, j.result_key()) || !c.get_meta(j.result_key());
}

Done restore(Console& c, const CompressJournal& j)
{
    // The old records back, only if what they list is still there.
    for (const auto& o : olds_of(j)) {
        const auto infos = record_contents(*o.second);
        if (!infos) return fail("an original record cannot be read");
        for (const ContentInfo& i : *infos)
            if (!c.has_content(i.id)) return fail("the original contents are gone: the old records cannot be put back");
    }
    for (const auto& o : olds_of(j))
        if (auto d = c.set_meta(o.first, *o.second); !d) return d;
    if (!is_old_key(j, j.result_key())) {
        if (auto d = c.remove_meta(j.result_key()); !d) return d;
    }
    if (auto d = c.commit_meta(); !d) return d;
    if (!is_as_before(c, j)) return fail("the original records did not come back");
    return {};
}

Done do_revert(Console& c, JournalStore& store, const CompressJournal& j)
{
    if (is_as_before(c, j)) {
        delete_ours(c, j);
        store.clear();
        return {};
    }
    const auto cur = c.get_meta(j.result_key());
    if (!cur || j.new_record.empty() || *cur != j.new_record) return fail("the title changed since: left as it is");
    if (auto d = restore(c, j); !d) return d;
    delete_ours(c, j);
    store.clear();
    return {};
}

Done do_keep(Console& c, JournalStore& store, const CompressJournal& j)
{
    const auto cur = c.get_meta(j.result_key());
    if (!cur || j.new_record.empty() || *cur != j.new_record) return fail("the title is not the compressed one: nothing to keep");
    for (const ContentId& id : j.contents)
        if (!c.has_content(id)) return fail("the new contents are missing: the old ones are kept");
    // A merged title is known to ns under its new key (said again: a restart may have come between the swap and the push).
    if (j.new_key) {
        if (auto d = c.push_application_record(j.key.id, *j.new_key); !d) return fail("the application record could not be updated: " + d.error());
    }
    // From here the old contents go: a restart finishes this rather than offering to go back.
    if (j.phase != CompressJournal::Phase::Keeping) {
        CompressJournal k = j;
        k.phase = CompressJournal::Phase::Keeping;
        if (!store.save(k.text())) return fail("the journal could not be kept: nothing was deleted");
    }
    std::vector<ContentId> old_ids = {j.old_program, j.old_meta};
    if (j.patch_key) {
        old_ids.push_back(j.old_patch_program);
        old_ids.push_back(j.old_patch_meta);
    }
    for (const ContentId& id : old_ids)
        if (!in_record(*cur, id) && c.has_content(id)) c.delete_content(id);
    store.clear();
    return {};
}

// The content meta of the game with its update merged in: the game's, at the
// update's version, requiring the firmware that reads the layer and the update's
// own requirement; its contents the update's where the update has some of a type
// (control data, legal text, documents, data) and the game's otherwise, and the
// merged program.
ContentMeta merged_meta(const ContentMeta& base, const ContentMeta& patch, const ContentInfo& program)
{
    ContentMeta m = base;
    m.version = patch.version;
    m.required_download_system_version = std::max(base.required_download_system_version, patch.required_download_system_version);
    if (m.extended_header.size() >= 12) {
        const uint32_t req = std::max({le32(m.extended_header.data() + 8), patch.required_system_version(), SYSTEM_VERSION_12_0_0});
        put_le32(m.extended_header.data() + 8, req);
    }
    m.extended_data.clear();
    m.contents.clear();
    m.contents.push_back(program);
    for (const ContentType t : {ContentType::Data, ContentType::Control, ContentType::HtmlDocument, ContentType::LegalInformation}) {
        std::vector<ContentInfo> from_patch, from_base;
        for (const ContentInfo& i : patch.contents)
            if (i.type == t) from_patch.push_back(i);
        for (const ContentInfo& i : base.contents)
            if (i.type == t) from_base.push_back(i);
        const auto& chosen = from_patch.empty() ? from_base : from_patch;
        m.contents.insert(m.contents.end(), chosen.begin(), chosen.end());
    }
    return m;
}
}   // namespace

// ---------------------------------------------------------------- journal

std::string CompressJournal::text() const
{
    char b[40];
    std::snprintf(b, sizeof b, "%016llx:%u:%02x", (unsigned long long)key.id, (unsigned)key.version, (unsigned)std::to_underlying(key.type));
    std::ostringstream out;
    out << "schema=1\n";
    out << "phase=" << (phase == Phase::Writing ? "writing" : phase == Phase::Swapped ? "swapped" : "keeping") << "\n";
    out << "key=" << b << "\n";
    out << "old_record=" << hex_of(old_record) << "\n";
    out << "new_record=" << hex_of(new_record) << "\n";
    out << "old_program=" << hex_of(old_program) << "\n";
    out << "old_meta=" << hex_of(old_meta) << "\n";
    auto key_text = [](const MetaKey& k) {
        char t[40];
        std::snprintf(t, sizeof t, "%016llx:%u:%02x", (unsigned long long)k.id, (unsigned)k.version, (unsigned)std::to_underlying(k.type));
        return std::string(t);
    };
    if (new_key) out << "new_key=" << key_text(*new_key) << "\n";
    if (patch_key) {
        out << "patch_key=" << key_text(*patch_key) << "\n";
        out << "patch_record=" << hex_of(patch_record) << "\n";
        out << "old_patch_program=" << hex_of(old_patch_program) << "\n";
        out << "old_patch_meta=" << hex_of(old_patch_meta) << "\n";
    }
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
            else if (v == "keeping") j.phase = Phase::Keeping;
            else return std::nullopt;
            phase_ok = true;
        } else if (k == "key" || k == "new_key" || k == "patch_key") {
            unsigned long long id;
            unsigned           version, type;
            if (std::sscanf(v.c_str(), "%16llx:%u:%2x", &id, &version, &type) != 3) return std::nullopt;
            const MetaKey mk{id, version, (MetaType)type};
            if (k == "key") {
                j.key = mk;
                key_ok = true;
            } else if (k == "new_key") {
                j.new_key = mk;
            } else {
                j.patch_key = mk;
            }
        } else if (k == "patch_record") {
            const auto b = bytes_of(v);
            if (!b || b->empty()) return std::nullopt;
            j.patch_record = *b;
        } else if (k == "old_record") {
            const auto b = bytes_of(v);
            if (!b || b->empty()) return std::nullopt;
            j.old_record = *b;
            old_ok = true;
        } else if (k == "new_record") {
            const auto b = bytes_of(v);
            if (!b) return std::nullopt;
            j.new_record = *b;
        } else if (k == "old_program" || k == "old_meta" || k == "old_patch_program" || k == "old_patch_meta") {
            const auto id = id_of_hex(v);
            if (!id) return std::nullopt;
            (k == "old_program" ? j.old_program : k == "old_meta" ? j.old_meta : k == "old_patch_program" ? j.old_patch_program : j.old_patch_meta) = *id;
        } else if (k == "placeholder" || k == "content") {
            const auto id = id_of_hex(v);
            if (!id) return std::nullopt;
            (k == "placeholder" ? j.placeholders : j.contents).push_back(*id);
        }
    }
    if (!schema_ok || !phase_ok || !key_ok || !old_ok) return std::nullopt;
    if (j.patch_key && j.patch_record.empty()) return std::nullopt;
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
    // Merging an update: its record, contents and content meta.
    bool                                 merging = false;
    MetaKey                              patch_key;
    std::vector<uint8_t>                 patch_record, patch_cnmt;
    ContentInfo                          patch_program, patch_meta;
    ContentMeta                          base_parsed, patch_parsed;
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
    if (m.options.update_installed && !m.options.merge_update) return fail("an update is installed: it would not install over the compressed game");
    if (m.key.type != MetaType::Application) return fail("only a base game is compressed");
    m.merging = m.options.merge_update.has_value();
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
    m.base_parsed = *parsed;

    // The update to merge: its record, its content meta, its program.
    uint64_t patch_program_size = 0;
    if (m.merging) {
        m.patch_key = *m.options.merge_update;
        if (m.patch_key.type != MetaType::Patch || (m.patch_key.id & ~0x1fffull) != (m.key.id & ~0x1fffull) || m.patch_key.id == m.key.id)
            return fail("not this game's update");
        const auto prec = c.get_meta(m.patch_key);
        if (!prec) return fail("the update is not installed here");
        m.patch_record = *prec;
        const auto pinfos = record_contents(m.patch_record);
        if (!pinfos) return fail(pinfos.error());
        int pprograms = 0, pmetas = 0;
        for (const ContentInfo& i : *pinfos) {
            if (i.type == ContentType::Program) {
                m.patch_program = i;
                pprograms++;
            } else if (i.type == ContentType::Meta) {
                m.patch_meta = i;
                pmetas++;
            }
        }
        if (pprograms != 1 || pmetas != 1) return fail("an update with other than one program and one content meta");
        const auto psize = c.content_size(m.patch_meta.id);
        if (!psize || *psize > MAX_META_NCA || *psize != m.patch_meta.size) return fail("the update's content meta NCA is not what its record says");
        std::vector<uint8_t> pnca((size_t)*psize);
        if (auto d = c.read_content(m.patch_meta.id, 0, pnca); !d) return fail("the update's content meta NCA could not be read: " + d.error());
        const auto pcnmt = nca_read_cnmt(pnca, m.keys);
        if (!pcnmt) return fail("the update's content meta: " + pcnmt.error());
        m.patch_cnmt = *pcnmt;
        const auto pparsed = cnmt_parse(m.patch_cnmt);
        if (!pparsed || pparsed->type != MetaType::Patch) return fail("the update's content meta is not a patch's");
        m.patch_parsed = *pparsed;
        bool plisted = false;
        for (const ContentInfo& i : pparsed->contents) plisted = plisted || (i.id == m.patch_program.id && i.type == ContentType::Program);
        if (!plisted) return fail("the update's program is not in its content meta");
        if (m.patch_key.version != pparsed->version) return fail("the update's record and content meta disagree on its version");
        const auto pps = c.content_size(m.patch_program.id);
        if (!pps || *pps != m.patch_program.size) return fail("the update's program NCA is not what its record says");
        patch_program_size = *pps;
    }

    // The program, made over once to see what it comes to.
    const auto prog_size = c.content_size(m.program.id);
    if (!prog_size || *prog_size != m.program.size) return fail("the program NCA is not what the record says");
    const ContentId prog_id = m.program.id;
    auto            game = std::make_shared<Cached>(Cached{c, prog_id, {}, ~0ull, *prog_size});
    m.rc = std::make_unique<ProgramRecompressor>(
        [game](uint64_t off, std::span<uint8_t> out) { return game->read(off, out); }, *prog_size, m.keys, m.compress, m.decompress,
        m.options.block_size);
    if (m.merging) {
        auto upd = std::make_shared<Cached>(Cached{c, m.patch_program.id, {}, ~0ull, patch_program_size});
        m.rc->with_update([upd](uint64_t off, std::span<uint8_t> out) { return upd->read(off, out); }, patch_program_size);
    }
    if (auto d = m.rc->inspect(); !d) return fail("this title cannot be compressed: " + d.error());
    // The RomFS to make the program from, against its own integrity tree: a damaged game, or an update read wrong, stops here.
    if (const Done src = m.rc->check_source([&](uint64_t d, uint64_t t) { m.progress(CompressStage::Measuring, d, t * 2); }, m.cb.cancel); !src)
        return fail(m.cancelled() ? "cancelled" : src.error());

    Sha256   body;
    uint64_t counted = 0;
    const Done measured = m.rc->build(
        [&](std::span<const uint8_t> b) {
            body.update(b);
            counted += b.size();
            return true;
        },
        [&](uint64_t d, uint64_t t) { m.progress(CompressStage::Measuring, t + d, t * 2); }, m.cb.cancel);
    if (!measured) return fail(m.cancelled() ? "cancelled" : measured.error());
    m.header = m.rc->new_header();
    m.new_size = m.rc->new_size();
    m.body_size = counted;
    m.body_digest = body.digest();
    if (m.new_size != NCA_FULL_HEADER + counted) return fail("the measured size is not the size made");

    rep = {};
    rep.program_before = *prog_size + patch_program_size;   // what keeping the result frees
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

// ncm makes a content's folder when a placeholder is created with that content id, and registering renames the placeholder into it. The
// new content's id is only known once it is written, so a placeholder under that id is made and dropped first: the folder stays.
static Done make_content_folder(Console& c, CompressJournal& j, JournalStore& store, const ContentId& id)
{
    if (c.has_placeholder(id)) c.delete_placeholder(id);
    j.placeholders.push_back(id);   // so a crash between the two calls leaves nothing behind
    if (!store.save(j.text())) return fail("the journal could not be kept");
    if (auto d = c.create_placeholder(id, 0); !d) return d;
    c.delete_placeholder(id);
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
    if (auto d = make_content_folder(console, *j, store, id); !d) return d;
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
    if (m.merging) {
        const auto now_patch = c.get_meta(m.patch_key);
        if (!now_patch || *now_patch != m.patch_record) return fail("the update changed since it was measured");
    }
    if (c.free_space() < m.rep_needed) return fail("not enough free space for the new copy beside the old one");

    // The journal first: with it, whatever happens next can be undone.
    m.j.emplace();
    CompressJournal& j = *m.j;
    j.phase = CompressJournal::Phase::Writing;
    j.key = m.key;
    j.old_record = m.record;
    j.old_program = m.program.id;
    j.old_meta = m.meta.id;
    if (m.merging) {
        j.new_key = MetaKey{m.key.id, m.patch_parsed.version, MetaType::Application};
        j.patch_key = m.patch_key;
        j.patch_record = m.patch_record;
        j.old_patch_program = m.patch_program.id;
        j.old_patch_meta = m.patch_meta.id;
    }
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
    if (auto d = make_content_folder(c, j, m.store, new_program); !d) return abandon("the new program's folder could not be made: " + d.error());
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
    std::vector<uint8_t> new_cnmt_bytes;
    ContentMeta          merged;
    if (m.merging) {
        merged = merged_meta(m.base_parsed, m.patch_parsed, now);
        std::array<uint8_t, 32> digest{};
        std::copy(m.cnmt.end() - 32, m.cnmt.end(), digest.begin());
        new_cnmt_bytes = cnmt_serialize(merged, digest);
        if (!cnmt_parse(new_cnmt_bytes)) return abandon("the merged content meta cannot be read back");
    } else {
        const auto new_cnmt = cnmt_with_program(m.cnmt, m.program.id, now);
        if (!new_cnmt) return abandon("the new content meta: " + new_cnmt.error());
        new_cnmt_bytes = *new_cnmt;
    }
    const auto new_meta_nca = meta_nca_with_cnmt(m.meta_nca, new_cnmt_bytes, m.keys);
    if (!new_meta_nca) return abandon("the new content meta NCA: " + new_meta_nca.error());
    ContentId new_meta{};
    if (auto d = m.write_new(placeholder_for(m.meta.id, 'm'), *new_meta_nca, &new_meta); !d) return abandon("the new content meta could not be written: " + d.error());
    {
        const auto back_size = c.content_size(new_meta);
        std::vector<uint8_t> back(new_meta_nca->size());
        if (!back_size || *back_size != back.size() || !c.read_content(new_meta, 0, back) || back != *new_meta_nca)
            return abandon("the new content meta does not read back as it was written");
        const auto cnmt_back = nca_read_cnmt(back, m.keys);
        if (!cnmt_back || *cnmt_back != new_cnmt_bytes) return abandon("the new content meta NCA does not hold the new content meta");
    }

    // The record: the old one with the two contents replaced; for a merge, the one the merged content meta gives.
    std::vector<uint8_t> record;
    if (m.merging) {
        ContentInfo meta_content;
        meta_content.id = new_meta;
        meta_content.size = new_meta_nca->size();
        meta_content.type = ContentType::Meta;
        record = cnmt_install_record(*cnmt_parse(new_cnmt_bytes), meta_content);
    } else {
        const auto rec = record_with_contents(m.record, m.key.type, {{m.program.id, new_program, m.new_size}, {m.meta.id, new_meta, new_meta_nca->size()}});
        if (!rec) return abandon("the new record: " + rec.error());
        record = *rec;
    }
    if (stopped()) return abandon("cancelled");
    j.new_record = record;
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
    const MetaKey result = j.result_key();
    if (auto d = c.set_meta(result, record); !d) return undo("the record could not be replaced: " + d.error());
    if (m.merging) {   // one title now, at the update's version
        if (auto d = c.remove_meta(m.key); !d) return undo("the game's old record could not be removed: " + d.error());
        if (auto d = c.remove_meta(m.patch_key); !d) return undo("the update's record could not be removed: " + d.error());
    }
    if (auto d = c.commit_meta(); !d) return undo("the record could not be committed: " + d.error());
    const auto after = c.get_meta(result);
    if (!after || *after != record) return undo("the record does not read back as it was written");
    if (m.merging && (c.get_meta(m.key) || c.get_meta(m.patch_key))) return undo("the old records are still there after the swap");
    std::vector<ContentId> must_exist = {new_program, new_meta, m.program.id, m.meta.id};
    if (m.merging) {
        must_exist.push_back(m.patch_program.id);
        must_exist.push_back(m.patch_meta.id);
        for (const auto* r : {&m.record, &m.patch_record, &record})
            if (const auto infos = record_contents(*r))
                for (const ContentInfo& i : *infos) must_exist.push_back(i.id);
    }
    for (const ContentId& id : must_exist)
        if (!c.has_content(id)) return undo("a content is missing after the swap");
    // ns: the title's application record learns the key it is under now (the old keys are not removed from it:
    // that takes a call this module does not make).
    if (m.merging) {
        if (auto d = c.push_application_record(m.key.id, result); !d) return undo("the application record could not be updated: " + d.error());
    }
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
    const auto cur = c.get_meta(j->result_key());
    if (cur && !j->new_record.empty() && *cur == j->new_record && j->phase == CompressJournal::Phase::Keeping) {
        // The user had kept it: finish deleting the old contents.
        (void)do_keep(c, store, *j);
        return Left{};
    }
    if (cur && !j->new_record.empty() && *cur == j->new_record) {
        Left l;
        l.found = Found::AwaitingDecision;
        l.key = j->result_key();
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
