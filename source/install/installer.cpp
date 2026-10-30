// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/installer.hpp"

#include <algorithm>
#include <cstring>

#include "install/content_stream.hpp"
#include "util/sha256.hpp"

namespace install
{
namespace
{
constexpr uint64_t MAX_META_FILE = 16u << 20;   // a content meta NCA: a few KiB
constexpr uint64_t MAX_TICKET_FILE = 64u << 10;   // a ticket: 0x2c0 bytes; a certificate chain: 0x700

bool fail(std::string* why, const std::string& text)
{
    if (why) *why = text;
    return false;
}

// "<32 hex digits>" of a content's file name, whatever its extension.
std::optional<ContentId> id_of_name(const std::string& name)
{
    const size_t dot = name.find('.');
    return content_id_from_hex(name.substr(0, dot));
}

// A content meta's required system version (major << 26 | minor << 20 |
// micro << 16 | …) as hosversionGet() counts: major << 16 | minor << 8 | micro.
uint32_t as_hos_version(uint32_t v) { return (v >> 26) << 16 | ((v >> 20) & 0x3f) << 8 | ((v >> 16) & 0xf); }
}   // namespace

bool Journal::operator==(const Journal& o) const
{
    const bool same_at = at.has_value() == o.at.has_value() &&
                         (!at || (at->offset == o.at->offset && at->nca_size == o.at->nca_size && at->sha == o.at->sha));
    return registered == o.registered && writing == o.writing && same_at;
}

Installer::Installer(Source& src_, Package pkg_, Console& console_, NcaKeys keys_, InstallOptions options_,
                     InstallCallbacks callbacks)
    : src(src_), pkg(std::move(pkg_)), console(console_), keys(std::move(keys_)), options(options_), cb(std::move(callbacks))
{
}

bool Installer::prepare(std::string* why)
{
    titles.clear();
    total = 0;
    const auto metas_files = pkg.metas();
    if (metas_files.empty()) return fail(why, "no content meta in the package");
    std::vector<ContentId> planned;
    for (const PackageFile* f : metas_files) {
        TitlePlan t;
        const auto id = id_of_name(f->name);
        if (!id) return fail(why, f->name + ": not named after its content id");
        if (f->size > MAX_META_FILE) return fail(why, f->name + ": too big for a content meta");
        std::vector<uint8_t> file((size_t)f->size);
        if (!src.read(f->offset, file.data(), file.size(), why)) return false;
        if (f->name.ends_with(".ncz")) {
            // Decompressed into memory: the NCA it stands for.
            ContentStream cs(
                keys.header_key.data(), std::nullopt, 0, nullptr,
                [&](uint64_t offset, const uint8_t* data, size_t size) {
                    if (offset != t.meta_nca.size() || t.meta_nca.size() + size > MAX_META_FILE) return false;
                    t.meta_nca.insert(t.meta_nca.end(), data, data + size);
                    return true;
                });
            if (!cs.feed(file) || !cs.finish()) return fail(why, f->name + ": " + cs.error());
        } else {
            t.meta_nca = std::move(file);
        }
        const auto bytes = nca_read_cnmt(t.meta_nca, keys);
        if (!bytes) return fail(why, f->name + ": " + bytes.error());
        auto meta = cnmt_parse(*bytes);
        if (!meta) return fail(why, f->name + ": " + meta.error());
        t.meta = std::move(*meta);
        t.meta_content.id = *id;
        t.meta_content.size = t.meta_nca.size();
        t.meta_content.type = ContentType::Meta;
        Sha256 sha;
        sha.update(t.meta_nca);
        t.meta_content.hash = sha.digest();

        const uint32_t required = as_hos_version(t.meta.required_system_version());
        if (required > console.system_version() && !options.ignore_required_system_version)
            return fail(why, "the title needs a newer system version (" + std::to_string(required >> 16) + "." +
                                 std::to_string((required >> 8) & 0xff) + "." + std::to_string(required & 0xff) + ")");

        auto plan_one = [&](const ContentInfo& c) {
            if (std::ranges::contains(planned, c.id)) return true;   // another title's too
            if (console.has_content(c.id)) return true;
            if (!pkg.content(c.id)) return fail(why, content_id_hex(c.id) + ": a content the package lacks");
            planned.push_back(c.id);
            t.to_write.push_back(c);
            total += c.size;
            return true;
        };
        if (!console.has_content(t.meta_content.id) &&
            !std::ranges::contains(planned, t.meta_content.id)) {
            planned.push_back(t.meta_content.id);
            t.to_write.push_back(t.meta_content);
            total += t.meta_content.size;
        }
        for (const ContentInfo& c : t.meta.installable())
            if (!plan_one(c)) return false;
        titles.push_back(std::move(t));
    }
    for (const PackageFile* tik : pkg.tickets()) {
        const std::string cert = tik->name.substr(0, tik->name.size() - 4) + ".cert";
        if (!pkg.find(cert)) return fail(why, tik->name + ": no certificate beside it");
        if (tik->size > MAX_TICKET_FILE || pkg.find(cert)->size > MAX_TICKET_FILE)
            return fail(why, tik->name + ": a ticket or certificate too big");
    }
    if (total > console.free_space()) return fail(why, "not enough free space");
    return true;
}

bool Installer::write_meta_content(const TitlePlan& t, std::string* why)
{
    const ContentId& id = t.meta_content.id;
    console.delete_placeholder(id);   // one left behind by an earlier attempt
    if (!console.create_placeholder(id, t.meta_nca.size(), why)) return false;
    made.push_back(id);
    if (!console.write_placeholder(id, 0, t.meta_nca.data(), t.meta_nca.size(), why)) return false;
    if (!console.register_placeholder(id, why)) return false;
    console.delete_placeholder(id);
    state.registered.push_back(id);
    save_journal();
    done += t.meta_nca.size();
    if (cb.progress) cb.progress(done, total, content_id_hex(id) + ".cnmt.nca");
    return true;
}

void Installer::save_journal()
{
    journaled = done;
    if (cb.journal) cb.journal(state);
}

bool Installer::write_content(const ContentInfo& c, std::string* why)
{
    const PackageFile* f = pkg.content(c.id);
    if (!f) return fail(why, content_id_hex(c.id) + ": a content the package lacks");
    // Taken up where an earlier run left it: its placeholder still there,
    // its stream's checkpoint kept (a plain .nca).
    const auto& r = resume;
    const bool  resuming = r && r->writing == c.id && r->at && r->at->offset <= f->size && console.has_placeholder(c.id);
    if (!resuming) console.delete_placeholder(c.id);
    std::string where;
    ContentStream cs(
        keys.header_key.data(), c.hash, c.size,
        [&](const NcaHeader& h, NcaSignature sig, std::string* refuse) {
            if (sig != NcaSignature::Valid && options.signatures != SignaturePolicy::Off && !signatures_accepted) {
                if (options.signatures == SignaturePolicy::Require || !cb.accept_signature ||
                    !cb.accept_signature(f->name)) {
                    *refuse = f->name + ": not signed by Nintendo";
                    return false;
                }
                signatures_accepted = true;
            }
            if (!console.create_placeholder(c.id, h.size, refuse)) return false;
            made.push_back(c.id);
            state.writing = c.id;
            state.at.reset();
            return true;
        },
        [&](uint64_t offset, const uint8_t* data, size_t size) {
            if (!console.write_placeholder(c.id, offset, data, size, &where)) return false;
            done += size;
            if (cb.progress) cb.progress(done, total, f->name);
            return true;
        });
    uint64_t from = 0;
    if (resuming && cs.resume_at(*r->at)) {
        from = r->at->offset;
        made.push_back(c.id);
        state.writing = c.id;
        state.at = r->at;
        done += from;
        resumed += from;
        journaled = done;
    } else if (resuming) {
        console.delete_placeholder(c.id);   // a checkpoint not this content's: from the start
    }
    const bool streamed = src.stream(f->offset + from, f->size - from,
                                     [&](const uint8_t* data, size_t size) {
                                         if (cancelled()) {
                                             where = "cancelled";
                                             return false;
                                         }
                                         if (pausing()) {
                                             where = "paused";
                                             return false;
                                         }
                                         if (!cs.feed({data, size})) return false;
                                         if (done - journaled >= JOURNAL_EVERY && cs.checkpoint()) {
                                             state.at = cs.checkpoint();
                                             save_journal();
                                         }
                                         return true;
                                     },
                                     &where);
    // The source itself may stop on the same request (a download cut short
    // at once, saying "cancelled" whichever it was): the pause or the
    // cancel, not a failure.
    if (!streamed && where != "paused") {
        if (cancelled()) where = "cancelled";
        else if (pausing()) where = "paused";
    }
    if (where == "paused") {
        // Kept to go on later when its stream can be taken up (a .nca);
        // a .ncz's placeholder is of no use: it starts again.
        state.at = cs.checkpoint();
        if (!state.at) {
            console.delete_placeholder(c.id);
            std::erase(made, c.id);
            state.writing.reset();
        }
        was_paused = true;
        save_journal();
        return fail(why, where);
    }
    if (!streamed || !cs.finish()) {
        if (where == "cancelled") return fail(why, where);
        // The console's own reason when a write failed, else the stream's.
        const std::string& detail = !where.empty() && where != "stopped" ? where : cs.error();
        // A refusal of its header already names the file.
        if (detail.compare(0, f->name.size() + 1, f->name + ":") == 0) return fail(why, detail);
        return fail(why, f->name + ": " + detail);
    }
    if (!console.register_placeholder(c.id, why)) return false;
    console.delete_placeholder(c.id);
    state.registered.push_back(c.id);
    state.writing.reset();
    state.at.reset();
    save_journal();
    return true;
}

bool Installer::import_tickets(std::string* why)
{
    for (const PackageFile* tik : pkg.tickets()) {
        const PackageFile*   cert = pkg.find(tik->name.substr(0, tik->name.size() - 4) + ".cert");
        std::vector<uint8_t> t((size_t)tik->size), c((size_t)cert->size);
        if (!src.read(tik->offset, t.data(), t.size(), why) || !src.read(cert->offset, c.data(), c.size(), why))
            return false;
        if (!console.import_ticket(t, c, why)) return false;
    }
    return true;
}

void Installer::undo()
{
    // Undone as far as it goes: a step that fails leaves nothing better to do.
    for (auto it = metas.rbegin(); it != metas.rend(); ++it) {
        std::string ignored;
        if (it->previous)
            (void)console.set_meta(it->key, *it->previous, &ignored);
        else
            (void)console.remove_meta(it->key, &ignored);
    }
    if (!metas.empty()) {
        std::string ignored;
        (void)console.commit_meta(&ignored);
    }
    metas.clear();
    for (const ContentId& id : made) {
        console.delete_placeholder(id);
        console.delete_content(id);
    }
    made.clear();
}

bool Installer::run(std::string* why)
{
    done = 0;
    resumed = 0;
    journaled = 0;
    was_paused = false;
    made.clear();
    metas.clear();
    state = Journal{};
    // The contents earlier runs registered: undone with this run's.
    if (resume) {
        state.registered = resume->registered;
        made = state.registered;
        // Its placeholder too, should this run end before it is taken up.
        if (resume->writing) made.push_back(*resume->writing);
    }
    std::string err;
    auto        stop = [&](const std::string& reason) {
        undo();
        return fail(why, reason);
    };
    auto pause = [&]() {
        was_paused = true;
        save_journal();
        return fail(why, "paused");
    };
    for (const TitlePlan& t : titles)
        for (const ContentInfo& c : t.to_write) {
            if (cancelled()) return stop("cancelled");
            if (pausing()) return pause();
            const bool ok = c.id == t.meta_content.id ? write_meta_content(t, &err) : write_content(c, &err);
            if (!ok && was_paused) return fail(why, err);
            if (!ok) return stop(err);
        }
    if (cancelled()) return stop("cancelled");
    if (!import_tickets(&err)) return stop("ticket: " + err);
    for (const TitlePlan& t : titles) {
        const MetaKey key{t.meta.title_id, t.meta.version, t.meta.type};
        metas.push_back(MetaBackup{key, console.get_meta(key)});
        const auto record = cnmt_install_record(t.meta, t.meta_content, options.ignore_required_system_version);
        if (!console.set_meta(key, record, &err)) return stop("content meta: " + err);
    }
    if (!console.commit_meta(&err)) return stop("content meta: " + err);
    for (const TitlePlan& t : titles) {
        const MetaKey key{t.meta.title_id, t.meta.version, t.meta.type};
        if (!console.push_application_record(t.meta.application_id(), key, &err)) return stop("application record: " + err);
    }
    made.clear();
    metas.clear();
    return true;
}

}   // namespace install
