// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "homebrew/install.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <unistd.h>

#include <aurore/extern/nlohmann/json.hpp>

#include "saves/archive.hpp"
#include "util/paths.hpp"
#include "util/sha256.hpp"

namespace homebrew
{

namespace
{
constexpr int    SCHEMA = 1;
constexpr size_t MAX_FILES = 4096;   // SPEC.md § 13: a manifest's entries

std::string str(const lantern::String& s) { return std::string(s.data(), s.size()); }

// A file's size and SHA-256 (hex), read in blocks; false when unreadable.
bool hash_file(const std::string& path, uint64_t* size, std::string* hex)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    Sha256         h;
    uint8_t        buf[64 * 1024];
    uint64_t       total = 0;
    size_t         n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
        h.update(std::span<const uint8_t>(buf, n));
        total += n;
    }
    const bool ok = !std::ferror(f);
    std::fclose(f);
    *size = total;
    *hex = h.hex();
    return ok;
}

bool copy_file(const std::string& from, const std::string& to)
{
    FILE* in = std::fopen(from.c_str(), "rb");
    if (!in) return false;
    FILE* out = std::fopen(to.c_str(), "wb");
    if (!out) {
        std::fclose(in);
        return false;
    }
    char   buf[64 * 1024];
    size_t n;
    bool   ok = true;
    while (ok && (n = std::fread(buf, 1, sizeof buf, in)) > 0) ok = std::fwrite(buf, 1, n, out) == n;
    ok = ok && !std::ferror(in);
    std::fclose(in);
    return std::fclose(out) == 0 && ok;
}

std::string parent_of(const std::string& path)
{
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "" : path.substr(0, slash);
}

std::string join(const std::string& root, const std::string& path)
{
    std::string r = root;
    while (!r.empty() && r.back() == '/') r.pop_back();
    return r + (path.empty() || path[0] != '/' ? "/" : "") + path;
}

// `from` made the file at `to`, replacing one that is there.
bool put_in_place(const std::string& from, const std::string& to, std::string* why)
{
    if (!paths::ensure_dir(parent_of(to))) {
        *why = "a folder could not be made for " + to;
        return false;
    }
    std::string e;
    if (!paths::replace_file(to, from, "", &e)) {
        *why = "could not write " + to + (e.empty() ? "" : ": " + e);
        return false;
    }
    return true;
}
}   // namespace

bool Book::load(const std::string& path)
{
    this->records.clear();
    this->newer = false;
    std::string text;
    if (!paths::read_file(path, text)) return true;   // none yet
    const auto doc = nlohmann::json::parse(text, nullptr, false);
    if (!doc.is_object() || !doc.contains("schema") || !doc["schema"].is_number_integer()) return false;
    if (doc["schema"].get<int>() > SCHEMA) {
        this->newer = true;
        return true;
    }
    if (!doc.contains("installed") || !doc["installed"].is_array()) return true;
    for (const auto& r : doc["installed"]) {
        if (!r.is_object() || !r.contains("id") || !r["id"].is_string() || this->records.size() >= MAX_RECORDS) continue;
        Record rec;
        rec.id = r["id"].get<std::string>();
        rec.version = r.value("version", std::string());
        rec.platform = r.value("platform", std::string());
        rec.server_uid = r.value("server", std::string());
        rec.build = r.value("build", uint64_t(0));
        if (r.contains("files") && r["files"].is_array())
            for (const auto& f : r["files"])
                if (f.is_string() && rec.files.size() < MAX_FILES) rec.files.push_back(f.get<std::string>());
        if (!rec.id.empty()) this->records[rec.id] = std::move(rec);
    }
    return true;
}

bool Book::save(const std::string& path) const
{
    if (this->newer) return false;   // never over a newer schema
    nlohmann::json list = nlohmann::json::array();
    for (const auto& [id, r] : this->records)
        list.push_back({ { "id", r.id }, { "version", r.version }, { "platform", r.platform }, { "server", r.server_uid },
                         { "build", r.build }, { "files", r.files } });
    nlohmann::json doc = { { "schema", SCHEMA }, { "installed", list } };
    return paths::atomic_write(path, doc.dump(1));
}

const Record* Book::find(const std::string& id) const
{
    const auto it = this->records.find(id);
    return it == this->records.end() ? nullptr : &it->second;
}

void Book::put(const Record& record)
{
    if (this->records.size() < MAX_RECORDS || this->records.count(record.id)) this->records[record.id] = record;
}

void Book::erase(const std::string& id) { this->records.erase(id); }

Outcome install(const lantern::Homebrew& entry, const std::string& package_file, const std::string& sd_root,
                const std::string& staging, const Record* old, const Consent& consent)
{
    Outcome out;
    const auto fail = [&out](const std::string& why) {
        out.why = why;
        return out;
    };

    const auto pick = lantern::pick_package(entry, "");
    if (!pick) return fail("no package fits this console: " + std::string(pick.error().detail()));
    const lantern::Package& package = **pick;
    const auto              plan = lantern::plan_install(entry, package);
    if (!plan) return fail("refused: " + std::string(plan.error().detail()));
    if (plan->placement == lantern::Placement::system_installer) return fail("this console has no installer for it");
    if (plan->files.empty() || plan->files.size() > MAX_FILES) return fail("the package lists no file, or too many");

    std::vector<std::string> asks;
    for (const auto& f : plan->files)
        if (f.needs_consent) asks.push_back(str(f.path));
    if (!asks.empty() && !(consent && consent(asks))) {
        out.declined = true;
        return fail("declined");
    }

    uint64_t    size = 0;
    std::string hex;
    if (!hash_file(package_file, &size, &hex)) return fail("the downloaded package could not be read");
    const auto want = lantern::to_hex(package.sha256);
    if (size != package.size || hex != std::string(want.data(), want.size())) return fail("the package is not the one announced");

    // Every file, as (path, source); an archive's come from `staging`.
    std::vector<std::pair<std::string, std::string>> moves;
    if (plan->placement == lantern::Placement::single_file) {
        // A copy, so the package stays for a later retry; a fresh name beside the target.
        const std::string target = join(sd_root, str(plan->files[0].path));
        if (!paths::ensure_dir(parent_of(target))) return fail("a folder could not be made for " + target);
        const std::string part = target + ".part";
        if (!copy_file(package_file, part)) {
            std::remove(part.c_str());
            return fail("could not write " + target);
        }
        std::string why;
        if (!put_in_place(part, target, &why)) {
            std::remove(part.c_str());
            return fail(why);
        }
    } else {
        std::string why;
        if (!save_archive::clear_dir(staging, &why) && paths::file_exists(staging)) return fail(why);
        if (!paths::ensure_dir(staging)) return fail("the staging folder could not be made");
        std::vector<save_archive::Entry> entries;
        if (!save_archive::read_entries(package_file, &entries, &why)) return fail("the archive is damaged: " + why);
        std::set<std::string> listed;
        for (const auto& e : entries)
            if (!e.folder) listed.insert(e.name);
        std::set<std::string> promised;
        for (const auto& f : plan->files) promised.insert(str(f.path).substr(1));   // "/switch/x" -> "switch/x"
        if (listed != promised) return fail("the archive does not hold exactly the files its manifest lists");
        if (!save_archive::unzip_to(package_file, staging, nullptr, nullptr, &why)) return fail("unpacking failed: " + why);
        for (const auto& f : plan->files) {
            const std::string name = str(f.path).substr(1), at = join(staging, name);
            uint64_t          n = 0;
            std::string       h;
            const auto        digest = lantern::to_hex(f.sha256);
            if (!hash_file(at, &n, &h) || n != f.size || h != std::string(digest.data(), digest.size()))
                return fail(name + " is not what the manifest says");
            moves.push_back({ str(f.path), at });
        }
        for (const auto& [path, from] : moves)
            if (!put_in_place(from, join(sd_root, path), &why)) return fail(why);
        (void)save_archive::clear_dir(staging, &why);
    }

    out.record.id = str(entry.id);
    out.record.version = str(entry.version);
    out.record.platform = str(entry.platform);
    out.record.build = entry.build;
    for (const auto& f : plan->files) out.record.files.push_back(str(f.path));

    // What the old version wrote and this one does not.
    if (old) {
        const std::set<std::string> now(out.record.files.begin(), out.record.files.end());
        for (const auto& p : old->files)
            if (!now.count(p) && std::remove(join(sd_root, p).c_str()) == 0) out.removed.push_back(p);
    }
    out.ok = true;
    return out;
}

void sweep_leftovers(const Book& book, const std::string& sd_root, const std::string& staging, const std::string& package_file)
{
    std::remove(package_file.c_str());
    std::string why;
    (void)save_archive::clear_dir(staging, &why);
    ::rmdir(staging.c_str());
    for (const auto& [id, r] : book.all())
        for (const auto& p : r.files) std::remove((join(sd_root, p) + ".part").c_str());
}

std::vector<std::string> uninstall(const Record& record, const std::string& sd_root)
{
    std::vector<std::string> gone;
    for (const auto& p : record.files)
        if (std::remove(join(sd_root, p).c_str()) == 0) gone.push_back(p);
    // The folders left empty, deepest first, but never above the first level
    // below "/switch" or "/config" (rmdir fails on a folder that holds anything).
    std::set<std::string, std::greater<>> dirs;
    for (const auto& p : record.files) {
        std::string d = parent_of(p);
        while (std::count(d.begin(), d.end(), '/') >= 2) {
            dirs.insert(d);
            d = parent_of(d);
        }
    }
    for (const auto& d : dirs) ::rmdir(join(sd_root, d).c_str());
    return gone;
}

}   // namespace homebrew
