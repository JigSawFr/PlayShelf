// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/services.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <format>
#include <map>
#include <sys/stat.h>

#include "lantern_app/file_sink.hpp"
#include "lantern_app/platform.hpp"
#include "library/catalog.hpp"

namespace lantern_app
{
namespace
{
constexpr uint64_t MAX_UPLOAD = 256u << 20;   // an archive sent from memory: 256 MiB at most

// A client signed in to the server, with the platform it runs on.
struct Link
{
    explicit Link(const library::Server& server) : connected(connect(server, platform.env(), platform.store())) {}
    Platform  platform;
    Connected connected;
    lantern::Client* client() { return connected.client ? &*connected.client : nullptr; }
};

std::unique_ptr<Link> open(const library::Server& server, lantern::Family family, std::string* why)
{
    auto link = std::make_unique<Link>(server);
    if (!link->client()) {
        *why = link->connected.why;
        return nullptr;
    }
    if (link->client()->offers(family) == 0) {
        *why = "this server does not offer it";
        return nullptr;
    }
    return link;
}

std::string hex_id(uint64_t id) { return std::format("{:016X}", id); }

bool parse_id(const std::string& text, uint64_t* out)
{
    if (text.size() != 16) return false;
    uint64_t v = 0;
    for (const char c : text) {
        const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return false;
        v = v << 4 | (uint64_t)d;
    }
    *out = v;
    return true;
}

// "2026-11-06T10:20:30Z" as POSIX time, 0 when it is not that.
int64_t posix_time(const std::string& iso)
{
    std::tm tm{};
    const char* end = ::strptime(iso.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
    return end ? (int64_t)::timegm(&tm) : 0;
}

std::string hex_of(const lantern::Digest& d)
{
    const auto h = lantern::to_hex(d);
    return std::string(h.data(), h.size());
}

uint64_t size_of(const std::string& path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 ? (uint64_t)st.st_size : 0;
}

library::SaveBackup backup_of(const lantern::Save& s)
{
    library::SaveBackup b;
    parse_id(std::string(s.title_id), &b.title_id);
    b.save_id    = std::string(s.save_id);
    b.note       = library::display_text(std::string(s.note), library::MAX_NOTE);
    b.created_at = std::string(s.created);
    b.created    = posix_time(b.created_at);
    b.size       = s.size;
    b.sha256     = hex_of(s.sha256);
    return b;
}

lantern::Save save_of(const library::SaveBackup& b)
{
    lantern::Save s;
    s.platform = "switch";
    s.title_id = hex_id(b.title_id);
    s.save_id  = b.save_id;
    s.size     = b.size;
    if (const auto d = lantern::digest_from_hex(b.sha256)) s.sha256 = *d;
    return s;
}
}   // namespace

bool list_cheats(const library::Server& server, uint64_t title_id, std::vector<library::Cheat>* out, std::string* why)
{
    auto link = open(server, lantern::Family::cheats, why);
    if (!link) return false;
    const std::string id = title_id ? hex_id(title_id) : "";
    lantern::CheatsQuery query;
    query.title_id = id;
    const auto sets = link->client()->cheats(query);
    if (!sets || !*sets) {
        *why = sets ? "the server lists its cheats in a newer version than this one reads" : describe(sets.error());
        return false;
    }
    out->clear();
    for (const lantern::CheatSet& c : (*sets)->cheats) {
        library::Cheat cheat;
        if (!parse_id(std::string(c.title_id), &cheat.title_id)) continue;
        cheat.build_id = library::normal_build_id(std::string(c.build_id));
        if (cheat.build_id.empty()) continue;
        cheat.note   = library::display_text(std::string(c.note), 160);
        cheat.id     = std::string(c.id);
        cheat.sha256 = hex_of(c.sha256);
        cheat.size   = c.size;
        out->push_back(std::move(cheat));
        if (out->size() >= 20000) break;
    }
    return true;
}

bool fetch_cheat(const library::Server& server, const library::Cheat& cheat, std::string* text, std::string* why)
{
    auto link = open(server, lantern::Family::cheats, why);
    if (!link) return false;
    if (cheat.size > library::MAX_CHEAT_BYTES) {
        *why = "this cheat file is too big";
        return false;
    }
    lantern::CheatSet set;
    set.id       = cheat.id;
    set.platform = "switch";
    set.title_id = hex_id(cheat.title_id);
    set.build_id = cheat.build_id;
    set.size     = cheat.size;
    if (const auto d = lantern::digest_from_hex(cheat.sha256)) set.sha256 = *d;
    const auto got = link->client()->cheat_text(set);
    if (!got) {
        *why = describe(got.error());
        return false;
    }
    text->assign(got->data(), got->size());
    return true;
}

bool list_saves(const library::Server& server, std::vector<library::SaveBackup>* out, std::string* why)
{
    auto link = open(server, lantern::Family::saves, why);
    if (!link) return false;
    const auto saves = link->client()->saves();
    if (!saves || !*saves) {
        *why = saves ? "the server lists its saves in a newer version than this one reads" : describe(saves.error());
        return false;
    }
    out->clear();
    for (const lantern::Save& s : (*saves)->saves) {
        library::SaveBackup b = backup_of(s);
        if (!b.title_id) continue;
        out->push_back(std::move(b));
        if (out->size() >= 20000) break;
    }
    // The newest backup of each game is the latest.
    std::map<uint64_t, size_t> newest;
    for (size_t i = 0; i < out->size(); i++) {
        const auto it = newest.find((*out)[i].title_id);
        if (it == newest.end() || (*out)[i].created > (*out)[it->second].created) newest[(*out)[i].title_id] = i;
    }
    for (const auto& [id, i] : newest) (*out)[i].latest = true;
    return true;
}

bool upload_save(const library::Server& server, uint64_t title_id, const std::string& zip_path, const std::string& note,
                 const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel, std::string* why)
{
    const uint64_t total = size_of(zip_path);
    if (total == 0 || total > MAX_UPLOAD) {
        *why = "the archive is empty or too big to send";
        return false;
    }
    std::vector<std::byte> bytes((size_t)total);
    FILE* f = std::fopen(zip_path.c_str(), "rb");
    if (!f || std::fread(bytes.data(), 1, bytes.size(), f) != bytes.size()) {
        if (f) std::fclose(f);
        *why = "the archive could not be read";
        return false;
    }
    std::fclose(f);
    if (cancel && cancel->load()) return false;
    auto link = open(server, lantern::Family::saves, why);
    if (!link) return false;
    progress(0, total);
    lantern::SaveInfo info;
    info.kind = "account";
    info.note = note;
    const std::string id = hex_id(title_id);
    const auto sent = link->client()->upload_save(id, bytes, info);
    if (!sent) {
        *why = describe(sent.error());
        return false;
    }
    progress(total, total);
    return true;
}

bool download_save(const library::Server& server, const library::SaveBackup& backup, const std::string& zip_path,
                   const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel, std::string* why)
{
    auto link = open(server, lantern::Family::saves, why);
    if (!link) return false;
    FILE* f = open_private(zip_path, false);
    if (!f) {
        *why = "the file could not be written";
        return false;
    }
    const uint64_t total = backup.size;
    FileSink       sink(f, cancel, [&](uint64_t n) { progress(n, total); }, 0);
    lantern::DownloadState state;
    const auto done = link->client()->download_save(save_of(backup), state, sink);
    std::fclose(f);
    if (!done) {
        std::remove(zip_path.c_str());
        *why = describe(done.error());
        return false;
    }
    return true;
}

bool delete_save(const library::Server& server, const library::SaveBackup& backup, std::string* why)
{
    auto link = open(server, lantern::Family::saves, why);
    if (!link) return false;
    const auto gone = link->client()->delete_save(save_of(backup));
    if (!gone) {
        *why = describe(gone.error());
        return false;
    }
    return true;
}

bool list_requests(const library::Server& server, std::vector<library::TitleRequest>* out, std::string* why)
{
    auto link = open(server, lantern::Family::requests, why);
    if (!link) return false;
    const auto list = link->client()->requests();
    if (!list || !*list) {
        *why = list ? "the server lists its requests in a newer version than this one reads" : describe(list.error());
        return false;
    }
    out->clear();
    for (const lantern::TitleRequest& r : (*list)->requests) {
        library::TitleRequest t;
        t.id = std::string(r.id);
        parse_id(std::string(r.title_id), &t.title_id);
        t.title_name = library::display_text(std::string(r.name), 128);
        t.status     = library::display_text(std::string(r.status), 32);
        t.created    = posix_time(std::string(r.created));
        out->push_back(std::move(t));
        if (out->size() >= 20000) break;
    }
    return true;
}

bool request_title(const library::Server& server, uint64_t title_id, std::string* message, std::string* why)
{
    auto link = open(server, lantern::Family::requests, why);
    if (!link) return false;
    lantern::RequestCreate request;
    request.platform = "switch";
    request.title_id = hex_id(title_id);
    const auto made = link->client()->request_title(request);
    if (!made) {
        *why = describe(made.error());
        return false;
    }
    message->clear();
    return true;
}

}   // namespace lantern_app
