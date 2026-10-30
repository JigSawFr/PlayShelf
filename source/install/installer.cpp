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

Done Installer::prepare()
{
    titles.clear();
    total = 0;
    const auto metas_files = pkg.metas();
    if (metas_files.empty()) return fail("no content meta in the package");
    std::vector<ContentId> planned;
    for (const PackageFile* f : metas_files) {
        TitlePlan t;
        const auto id = id_of_name(f->name);
        if (!id) return fail(f->name + ": not named after its content id");
        if (f->size > MAX_META_FILE) return fail(f->name + ": too big for a content meta");
        std::vector<uint8_t> file((size_t)f->size);
        if (const Done r = src.read(f->offset, file.data(), file.size()); !r) return r;
        if (f->name.ends_with(".ncz")) {
            // Decompressed into memory: the NCA it stands for.
            ContentStream cs(
                keys.header_key.data(), std::nullopt, 0, nullptr,
                [&](uint64_t offset, const uint8_t* data, size_t size) {
                    if (offset != t.meta_nca.size() || t.meta_nca.size() + size > MAX_META_FILE) return false;
                    t.meta_nca.insert(t.meta_nca.end(), data, data + size);
                    return true;
                });
            if (!cs.feed(file) || !cs.finish()) return fail(f->name + ": " + cs.error());
        } else {
            t.meta_nca = std::move(file);
        }
        const auto bytes = nca_read_cnmt(t.meta_nca, keys);
        if (!bytes) return fail(f->name + ": " + bytes.error());
        auto meta = cnmt_parse(*bytes);
        if (!meta) return fail(f->name + ": " + meta.error());
        t.meta = std::move(*meta);
        t.meta_content.id = *id;
        t.meta_content.size = t.meta_nca.size();
        t.meta_content.type = ContentType::Meta;
        Sha256 sha;
        sha.update(t.meta_nca);
        t.meta_content.hash = sha.digest();

        const uint32_t required = as_hos_version(t.meta.required_system_version());
        if (required > console.system_version() && !options.ignore_required_system_version)
            return fail("the title needs a newer system version (" + std::to_string(required >> 16) + "." +
                        std::to_string((required >> 8) & 0xff) + "." + std::to_string(required & 0xff) + ")");

        auto plan_one = [&](const ContentInfo& c) -> Done {
            if (std::ranges::contains(planned, c.id)) return {};   // another title's too
            if (console.has_content(c.id)) return {};
            if (!pkg.content(c.id)) return fail(content_id_hex(c.id) + ": a content the package lacks");
            planned.push_back(c.id);
            t.to_write.push_back(c);
            total += c.size;
            return {};
        };
        if (!console.has_content(t.meta_content.id) &&
            !std::ranges::contains(planned, t.meta_content.id)) {
            planned.push_back(t.meta_content.id);
            t.to_write.push_back(t.meta_content);
            total += t.meta_content.size;
        }
        for (const ContentInfo& c : t.meta.installable())
            if (const Done r = plan_one(c); !r) return r;
        titles.push_back(std::move(t));
    }
    for (const PackageFile* tik : pkg.tickets()) {
        const std::string cert = tik->name.substr(0, tik->name.size() - 4) + ".cert";
        if (!pkg.find(cert)) return fail(tik->name + ": no certificate beside it");
        if (tik->size > MAX_TICKET_FILE || pkg.find(cert)->size > MAX_TICKET_FILE)
            return fail(tik->name + ": a ticket or certificate too big");
    }
    if (total > console.free_space()) return fail("not enough free space");
    return {};
}

Done Installer::write_meta_content(const TitlePlan& t)
{
    const ContentId& id = t.meta_content.id;
    console.delete_placeholder(id);   // one left behind by an earlier attempt
    if (const Done r = console.create_placeholder(id, t.meta_nca.size()); !r) return r;
    made.push_back(id);
    if (const Done r = console.write_placeholder(id, 0, t.meta_nca.data(), t.meta_nca.size()); !r) return r;
    if (const Done r = console.register_placeholder(id); !r) return r;
    console.delete_placeholder(id);
    state.registered.push_back(id);
    save_journal();
    done += t.meta_nca.size();
    if (cb.progress) cb.progress(done, total, content_id_hex(id) + ".cnmt.nca");
    return {};
}

void Installer::save_journal()
{
    journaled = done;
    if (cb.journal) cb.journal(state);
}

Done Installer::write_content(const ContentInfo& c)
{
    const PackageFile* f = pkg.content(c.id);
    if (!f) return fail(content_id_hex(c.id) + ": a content the package lacks");
    // Taken up where an earlier run left it: its placeholder still there,
    // its stream's checkpoint kept (a plain .nca).
    const auto& r = resume;
    const bool  resuming = r && r->writing == c.id && r->at && r->at->offset <= f->size && console.has_placeholder(c.id);
    if (!resuming) console.delete_placeholder(c.id);
    std::string where;
    ContentStream cs(
        keys.header_key.data(), c.hash, c.size,
        [&](const NcaHeader& h, NcaSignature sig) -> Done {
            if (sig != NcaSignature::Valid && options.signatures != SignaturePolicy::Off && !signatures_accepted) {
                if (options.signatures == SignaturePolicy::Require || !cb.accept_signature ||
                    !cb.accept_signature(f->name)) {
                    return fail(f->name + ": not signed by Nintendo");
                }
                signatures_accepted = true;
            }
            if (const Done made_one = console.create_placeholder(c.id, h.size); !made_one) return made_one;
            made.push_back(c.id);
            state.writing = c.id;
            state.at.reset();
            return {};
        },
        [&](uint64_t offset, const uint8_t* data, size_t size) {
            if (const Done wrote = console.write_placeholder(c.id, offset, data, size); !wrote) {
                where = wrote.error();
                return false;
            }
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
    const Done streamed = src.stream(f->offset + from, f->size - from,
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
                                     });
    // What the sink said when it stopped the stream (a write that failed,
    // the cancel, the pause) stands over the source's "stopped".
    if (!streamed && (where.empty() || streamed.error() != "stopped")) where = streamed.error();
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
        return fail(where);
    }
    if (!streamed || !cs.finish()) {
        if (where == "cancelled") return fail(where);
        // The console's own reason when a write failed, else the stream's.
        const std::string& detail = !where.empty() && where != "stopped" ? where : cs.error();
        // A refusal of its header already names the file.
        if (detail.compare(0, f->name.size() + 1, f->name + ":") == 0) return fail(detail);
        return fail(f->name + ": " + detail);
    }
    if (const Done registered = console.register_placeholder(c.id); !registered) return registered;
    console.delete_placeholder(c.id);
    state.registered.push_back(c.id);
    state.writing.reset();
    state.at.reset();
    save_journal();
    return {};
}

Done Installer::import_tickets()
{
    for (const PackageFile* tik : pkg.tickets()) {
        const PackageFile*   cert = pkg.find(tik->name.substr(0, tik->name.size() - 4) + ".cert");
        std::vector<uint8_t> t((size_t)tik->size), c((size_t)cert->size);
        if (const Done r = src.read(tik->offset, t.data(), t.size()); !r) return r;
        if (const Done r = src.read(cert->offset, c.data(), c.size()); !r) return r;
        if (const Done r = console.import_ticket(t, c); !r) return r;
    }
    return {};
}

void Installer::undo()
{
    // Undone as far as it goes: a step that fails leaves nothing better to do.
    for (auto it = metas.rbegin(); it != metas.rend(); ++it) {
        if (it->previous)
            (void)console.set_meta(it->key, *it->previous);
        else
            (void)console.remove_meta(it->key);
    }
    if (!metas.empty()) (void)console.commit_meta();
    metas.clear();
    for (const ContentId& id : made) {
        console.delete_placeholder(id);
        console.delete_content(id);
    }
    made.clear();
}

Done Installer::run()
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
    auto stop = [&](const std::string& reason) {
        undo();
        return fail(reason);
    };
    auto pause = [&]() {
        was_paused = true;
        save_journal();
        return fail("paused");
    };
    for (const TitlePlan& t : titles)
        for (const ContentInfo& c : t.to_write) {
            if (cancelled()) return stop("cancelled");
            if (pausing()) return pause();
            const Done wrote = c.id == t.meta_content.id ? write_meta_content(t) : write_content(c);
            if (!wrote && was_paused) return wrote;
            if (!wrote) return stop(wrote.error());
        }
    if (cancelled()) return stop("cancelled");
    if (const Done r = import_tickets(); !r) return stop("ticket: " + r.error());
    for (const TitlePlan& t : titles) {
        const MetaKey key{t.meta.title_id, t.meta.version, t.meta.type};
        metas.push_back(MetaBackup{key, console.get_meta(key)});
        const auto record = cnmt_install_record(t.meta, t.meta_content, options.ignore_required_system_version);
        if (const Done r = console.set_meta(key, record); !r) return stop("content meta: " + r.error());
    }
    if (const Done r = console.commit_meta(); !r) return stop("content meta: " + r.error());
    for (const TitlePlan& t : titles) {
        const MetaKey key{t.meta.title_id, t.meta.version, t.meta.type};
        if (const Done r = console.push_application_record(t.meta.application_id(), key); !r)
            return stop("application record: " + r.error());
    }
    made.clear();
    metas.clear();
    return {};
}

}   // namespace install
