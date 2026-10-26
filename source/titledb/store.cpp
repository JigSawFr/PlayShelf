// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "titledb/store.hpp"

#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

#include <borealis/extern/nlohmann/json.hpp>

#include "titledb/list.hpp"
#include "titledb/manifest.hpp"
#include "util/json_util.hpp"
#include "util/paths.hpp"
#include "util/sha256.hpp"

namespace titledb
{
namespace
{
constexpr size_t MAX_MANIFEST = 64u << 10;

uint64_t size_of(const std::string& path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) ? (uint64_t)st.st_size : 0;
}

std::string hash_of(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "";
    Sha256               h;
    std::vector<uint8_t> buf(1u << 20);
    size_t               n = 0;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0) h.update(buf.data(), n);
    const bool failed = std::ferror(f) != 0;
    std::fclose(f);
    return failed ? "" : h.hex();
}

// A pack's file name: what it is and its hash.
std::string file_name(const char* kind, const PackFile& p)
{
    return std::string(kind) + "-" + p.sha256.substr(0, 16) + ".pack";
}

bool ends_with(const std::string& s, const char* e)
{
    const std::string x(e);
    return s.size() >= x.size() && s.compare(s.size() - x.size(), x.size(), x) == 0;
}

// The pack fetched into `dir` unless it is there; false (why set) when it
// does not arrive whole and as its hash says.
bool fetch(const char* kind, const PackFile& p, const std::string& dir, const Net& net,
           const std::function<void(uint64_t)>& moved, std::string* why)
{
    const std::string path = dir + "/" + file_name(kind, p);
    if (size_of(path) == p.size) return true;
    const std::string part = path + ".part";
    uint64_t          have = size_of(part);
    if (have >= p.size) {
        std::remove(part.c_str());
        have = 0;
    }
    std::string w;
    if (!net.file(p.url, part, p.size, have, moved, &w)) {
        if (why) *why = std::string(kind) + " pack: " + (w.empty() ? "not fetched" : w);
        return false;
    }
    if (size_of(part) != p.size || hash_of(part) != p.sha256) {
        std::remove(part.c_str());
        if (why) *why = std::string(kind) + " pack: its SHA-256 differs from the manifest's";
        return false;
    }
    std::remove(path.c_str());
    if (std::rename(part.c_str(), path.c_str()) != 0) {
        if (why) *why = std::string(kind) + " pack: cannot be put in place";
        return false;
    }
    return true;
}

bool write_state(const std::string& dir, const State& st, std::string* why)
{
    nlohmann::json j;
    j["url"] = st.url;
    j["version"] = st.version;
    j["titles"] = st.titles;
    j["icons"] = st.icons;
    j["count"] = st.count;
    j["list"] = st.list;
    return paths::atomic_write(dir + "/state.json", j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) + "\n", why);
}

// A title list's source: its titles pack written from it, named after the
// list's hash (the same list: nothing written again).
Outcome from_list(const std::string& url, const std::string& dir, const std::string& text,
                  const std::vector<ListEntry>& entries, std::string* why)
{
    if (!paths::ensure_dir(dir)) {
        if (why) *why = "its folder cannot be made";
        return Outcome::Failed;
    }
    Sha256 h;
    h.update(text.data(), text.size());
    State next;
    next.url = url;
    next.version = h.hex().substr(0, 16);
    next.titles = "titles-" + next.version + ".pack";
    next.list = true;
    next.count = entries.size();
    State now;
    if (read_state(dir, &now) && now.version == next.version && now.titles == next.titles && size_of(dir + "/" + now.titles) > 0)
        return Outcome::Current;
    std::string w;
    if (!paths::atomic_write(dir + "/" + next.titles, titles_pack_of(entries), &w)) {
        if (why) *why = "titles pack: " + w;
        return Outcome::Failed;
    }
    if (!write_state(dir, next, &w)) {
        if (why) *why = "state: " + w;
        return Outcome::Failed;
    }
    prune(dir, true);
    return Outcome::Updated;
}
}   // namespace

bool read_state(const std::string& dir, State* out)
{
    std::string text;
    if (!paths::read_file(dir + "/state.json", text)) return false;
    nlohmann::json j;
    if (!json_util::parse(text, &j) || !j.is_object()) return false;
    *out = {};
    out->url = json_util::string_at(j, "url");
    out->version = json_util::string_at(j, "version");
    out->titles = json_util::string_at(j, "titles");
    out->icons = json_util::string_at(j, "icons");
    out->count = (size_t)json_util::integer_or(j, "count");
    out->list = json_util::flag_at(j, "list");
    // Names in the folder only.
    for (const std::string* n : { &out->titles, &out->icons })
        if (n->find('/') != std::string::npos || n->find("..") != std::string::npos) return false;
    return !out->titles.empty();
}

std::string source_dir(const std::string& root, const std::string& url)
{
    Sha256 h;
    h.update(url.data(), url.size());
    return root + "/" + h.hex().substr(0, 16);
}

Outcome update(const std::string& url, const std::string& dir, const Net& net,
               const std::function<void(uint64_t, uint64_t)>& progress, std::string* why)
{
    std::string text, w;
    if (!net.text(url, &text, &w)) {
        if (why) *why = "manifest: " + (w.empty() ? "not fetched" : w);
        return Outcome::Failed;
    }
    Manifest m;
    if (text.size() > MAX_MANIFEST || !parse_manifest(text, url, &m, &w)) {
        // Not a manifest: a title list, perhaps.
        std::string            list_why;
        std::vector<ListEntry> entries;
        if (parse_title_list(text, &entries, &list_why)) return from_list(url, dir, text, entries, why);
        if (why) *why = "manifest: " + (text.size() > MAX_MANIFEST ? std::string("too large") : w);
        return Outcome::Failed;
    }
    if (!paths::ensure_dir(dir)) {
        if (why) *why = "its folder cannot be made";
        return Outcome::Failed;
    }
    State next;
    next.url = url;
    next.version = m.version;
    next.titles = file_name("titles", m.titles);
    next.icons = m.icons.known() ? file_name("icons", m.icons) : "";
    State      now;
    const bool had = read_state(dir, &now);
    if (had && now.version == next.version && now.titles == next.titles && now.icons == next.icons &&
        size_of(dir + "/" + now.titles) == m.titles.size && (!m.icons.known() || size_of(dir + "/" + now.icons) == m.icons.size))
        return Outcome::Current;

    // What is left to fetch, and room for it.
    uint64_t total = 0;
    for (const auto& [kind, p] : { std::pair<const char*, const PackFile*>{ "titles", &m.titles }, { "icons", &m.icons } })
        if (p->known() && size_of(dir + "/" + file_name(kind, *p)) != p->size) {
            const uint64_t part = size_of(dir + "/" + file_name(kind, *p) + ".part");
            total += p->size - (part < p->size ? part : 0);
        }
    if (net.free_space && total && net.free_space() < total + (8u << 20)) {
        if (why) *why = "not enough free space";
        return Outcome::Failed;
    }
    uint64_t done = 0, base = 0;
    auto     moved = [&](uint64_t at) {
        done = base + at;
        if (progress) progress(done < total ? done : total, total);
    };
    for (const auto& [kind, p] : { std::pair<const char*, const PackFile*>{ "titles", &m.titles }, { "icons", &m.icons } }) {
        if (!p->known()) continue;
        if (!fetch(kind, *p, dir, net, moved, why)) return Outcome::Failed;
        base = done;
    }
    // The titles pack read, to be sure it is one, and its count kept.
    TitlePack t;
    if (!t.open(dir + "/" + next.titles, &w)) {
        if (why) *why = "titles pack: " + w;
        std::remove((dir + "/" + next.titles).c_str());
        return Outcome::Failed;
    }
    next.count = t.count();
    if (!write_state(dir, next, &w)) {
        if (why) *why = "state: " + w;
        return Outcome::Failed;
    }
    prune(dir, true);
    return Outcome::Updated;
}

void prune(const std::string& dir, bool parts)
{
    State s;
    const bool has = read_state(dir, &s);
    for (const std::string& name : paths::list_files(dir, "")) {
        if (!ends_with(name, ".pack") && !ends_with(name, ".pack.part")) continue;
        if (ends_with(name, ".pack.part") && !parts) continue;
        if (has && (name == s.titles || name == s.icons)) continue;
        std::remove((dir + "/" + name).c_str());
    }
}

bool remove_source(const std::string& dir)
{
    for (const std::string& name : paths::list_files(dir, ""))
        if (name[0] != '.') std::remove((dir + "/" + name).c_str());
    return ::rmdir(dir.c_str()) == 0;
}

void Database::open(const std::vector<std::string>& dirs)
{
    this->list.clear();
    this->skipped.clear();
    for (const std::string& dir : dirs) {
        Source s;
        if (!read_state(dir, &s.state)) continue;
        std::string why;
        s.titles = std::make_unique<TitlePack>();
        if (!s.titles->open(dir + "/" + s.state.titles, &why)) {
            this->skipped.push_back(s.state.url + ": " + why);
            continue;
        }
        if (!s.state.icons.empty()) {
            s.icons = std::make_unique<IconPack>();
            if (!s.icons->open(dir + "/" + s.state.icons, &why)) s.icons.reset();
        }
        this->list.push_back(std::move(s));
    }
}

std::optional<Title> Database::title(uint64_t id) const
{
    std::optional<Title> other;
    for (const Source& s : this->list) {
        auto t = s.titles->find(id);
        if (!t) continue;
        if (!t->name.empty()) return t;
        if (!other) other = std::move(t);
    }
    return other;
}

bool Database::icon(uint64_t id, std::vector<uint8_t>* bytes, std::string* ext) const
{
    for (const Source& s : this->list)
        if (s.icons && s.icons->find(id, bytes, ext)) return true;
    return false;
}

}   // namespace titledb
