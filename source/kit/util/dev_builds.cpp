// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/dev_builds.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <set>

#include <aurore/extern/nlohmann/json.hpp>

#include <aurore/cadran/util/safe_text.hpp>
#include <aurore/cadran/util/text.hpp>

#include "app_id.h"
#include "util/date.hpp"
#include "util/json_util.hpp"
#include "util/number.hpp"
#include "util/paths.hpp"
#include "util/sha256.hpp"
#include "util/update.hpp"

namespace dev_builds
{

namespace
{
using nlohmann::json;

bool is_hex(const std::string& s)
{
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isxdigit((unsigned char)c) != 0; });
}

// A full commit hash -> its first 7 digits; empty when it is not one.
std::string short_commit(const std::string& sha)
{
    return sha.size() >= 7 && sha.size() <= 64 && is_hex(sha) ? text::lower_ascii(sha.substr(0, 7)) : "";
}

bool https(const std::string& url) { return text::starts_with(url, "https://"); }

// Shown text: clean (util/safe_text.hpp), on one line.
constexpr size_t MAX_TITLE = 300;
std::string line_text(const std::string& text, size_t max = MAX_TITLE)
{
    return safe_text::clean(text, max, false);
}

// A time as GitHub writes it ("2026-10-09T12:03:00Z"), or nothing.
std::string iso_time(const std::string& text)
{
    int64_t unused;
    return text.size() <= 32 && date::iso_seconds(text, &unused) ? line_text(text, 32) : "";
}

// A page on GitHub, or nothing.
std::string page_url(const std::string& url)
{
    return text::starts_with(url, "https://github.com/") && url.size() < 512 ? safe_text::clean(url, 512, false) : "";
}

// Notes: clean, at most MAX_NOTES bytes, cut after a whole line.
std::string cut_notes(const std::string& text)
{
    return safe_text::clean_notes(text, MAX_NOTES);
}

// The "id" of the object j[key] (a repository); 0 when there is none.
int64_t id_at(const json& j, const char* key)
{
    const json* o = json_util::object_at(j, key);
    return o ? json_util::integer_or(*o, "id") : 0;
}

// Where fetch() finds builds: a release asset on github.com, an artifact on
// the API. A list kept on the SD card can be edited: nothing else is taken.
bool github_url(const std::string& url)
{
    return text::starts_with(url, "https://github.com/") || text::starts_with(url, "https://api.github.com/");
}
}   // namespace

namespace
{
// A release's <app>.nro, with its notes. False when it has none (or a
// malformed one), or is a draft; a pre-release only when `prerelease_ok`.
bool release_build(const json& r, bool prerelease_ok, Build* out)
{
    if (!r.is_object() || r.value("draft", true)) return false;
    const bool prerelease = r.value("prerelease", true);
    if (prerelease && !prerelease_ok) return false;
    const json* assets = json_util::array_at(r, "assets");
    if (!assets) return false;
    const std::string tag = json_util::string_at(r, "tag_name");
    for (const auto& a : *assets) {
        if (!a.is_object() || json_util::string_at(a, "name") != APP_ID ".nro") continue;
        Build b;
        b.kind = Kind::Release;
        b.prerelease = prerelease;
        b.version = tag.size() > 1 && tag[0] == 'v' ? tag.substr(1) : tag;
        if (!safe_text::is_version(b.version)) return false;
        b.page = page_url(json_util::string_at(r, "html_url"));
        b.url = json_util::string_at(a, "browser_download_url");
        b.date = iso_time(json_util::string_at(a, "updated_at"));
        b.sha256 = digest_hex(json_util::string_at(a, "digest"));
        b.notes = cut_notes(json_util::string_at(r, "body"));
        const int64_t size = json_util::integer_or(a, "size");
        if (b.version.empty() || !https(b.url) || size <= 0) return false;
        b.size = (uint64_t)size;
        *out = b;
        return true;
    }
    return false;
}
}   // namespace

bool parse_release(const std::string& text, Build* out)
{
    return json_util::parse_with(text, [out](const json& r) { return release_build(r, false, out); });
}

bool parse_releases(const std::string& text, std::vector<Build>* out, size_t keep)
{
    return json_util::parse_with(text, [out, keep](const json& j) {
        if (!j.is_array()) return false;
        std::set<std::string> seen;
        for (const auto& r : j) {
            if (out->size() >= keep) break;
            Build b;
            if (!release_build(r, true, &b) || !seen.insert(b.version).second) continue;
            out->push_back(b);
        }
        return true;
    });
}

bool is_build_artifact(const std::string& name)
{
    // A shim: the runs before 1.0.0 (2026-10-12) named it so, and their
    // artifacts expire 90 days later (GitHub's default; the -nro upload sets
    // no retention-days). This line can go in the first release after
    // 2027-01-10, its test with it.
    if (name == APP_ID "_release") return true;
    const std::string head = APP_ID "-", tail = "-nro";
    return name.size() > head.size() + tail.size() && text::starts_with(name, head) && text::ends_with(name, tail);
}

bool parse_artifacts(const std::string& text, std::vector<Artifact>* out, size_t* listed)
{
    return json_util::parse_with(text, [out, listed](const json& j) {
        const json* artifacts = json_util::array_at(j, "artifacts");
        if (!artifacts) return false;
        if (listed) *listed = artifacts->size();
        for (const auto& a : *artifacts) {
            if (!a.is_object() || !is_build_artifact(json_util::string_at(a, "name")) || a.value("expired", true)) continue;
            const json* workflow_run = json_util::object_at(a, "workflow_run");
            if (!workflow_run) continue;
            const auto& run = *workflow_run;
            Artifact x;
            x.url = json_util::string_at(a, "archive_download_url");
            x.date = iso_time(json_util::string_at(a, "created_at"));
            x.sha256 = digest_hex(json_util::string_at(a, "digest"));
            x.expires = iso_time(json_util::string_at(a, "expires_at"));
            x.run = json_util::integer_or(run, "id");
            x.branch = line_text(json_util::string_at(run, "head_branch"));
            x.commit = short_commit(json_util::string_at(run, "head_sha"));
            x.repo = json_util::integer_or(run, "repository_id");
            x.head_repo = json_util::integer_or(run, "head_repository_id");
            const int64_t size = json_util::integer_or(a, "size_in_bytes");
            if (!https(x.url) || size <= 0 || x.commit.empty() || x.branch.empty() || !x.repo || !x.head_repo) continue;
            x.size = (uint64_t)size;
            out->push_back(x);
        }
        return true;
    });
}

namespace
{
bool pull_of(const json& p, Pull* out)
{
    const json* head_side = json_util::object_at(p, "head");
    if (!head_side) return false;
    const auto& head = *head_side;
    const json* base = json_util::object_at(p, "base");
    Pull x;
    x.number = (int)json_util::integer_or(p, "number");
    x.title = line_text(json_util::string_at(p, "title"));
    x.body = cut_notes(json_util::string_at(p, "body"));
    x.page = page_url(json_util::string_at(p, "html_url"));
    x.branch = line_text(json_util::string_at(head, "ref"));
    x.head_sha = json_util::string_at(head, "sha");
    // A fork that was deleted has no repository: its builds cannot be matched.
    x.head_repo = id_at(head, "repo");
    x.repo = base ? id_at(*base, "repo") : 0;
    if (x.number <= 0 || x.branch.empty() || !x.head_repo) return false;
    *out = x;
    return true;
}

Ci ci_of(const json& r)
{
    if (json_util::string_at(r, "status") != "completed") return Ci::Running;
    return json_util::string_at(r, "conclusion") == "success" ? Ci::Passed : Ci::Failed;
}
}   // namespace

bool parse_pulls(const std::string& text, std::vector<Pull>* out)
{
    return json_util::parse_with(text, [out](const json& j) {
        if (!j.is_array()) return false;
        for (const auto& p : j) {
            Pull x;
            if (pull_of(p, &x)) out->push_back(x);
        }
        return true;
    });
}

bool parse_pull(const std::string& text, Pull* out)
{
    return json_util::parse_with(text, [out](const json& j) { return pull_of(j, out); });
}

bool parse_runs(const std::string& text, std::vector<Run>* out)
{
    return json_util::parse_with(text, [out](const json& j) {
        const json* runs = json_util::array_at(j, "workflow_runs");
        if (!runs) return false;
        for (const auto& r : *runs) {
            if (!r.is_object()) continue;
            Run x;
            x.id = json_util::integer_or(r, "id");
            x.sha = json_util::string_at(r, "head_sha");
            x.ci = ci_of(r);
            x.repo = id_at(r, "repository");
            x.head_repo = id_at(r, "head_repository");
            x.branch = line_text(json_util::string_at(r, "head_branch"));
            x.title = line_text(json_util::string_at(r, "display_title"));
            if (const json* commit = json_util::object_at(r, "head_commit"))
                x.message = cut_notes(json_util::string_at(*commit, "message"));
            if (x.id > 0 && short_commit(x.sha) != "") out->push_back(x);
        }
        return true;
    });
}

void apply_runs(std::vector<Build>* builds, const std::vector<Run>& runs)
{
    for (Build& b : *builds) {
        if (!b.artifact || !b.run) continue;
        for (const Run& r : runs) {
            if (r.id != b.run) continue;
            b.ci = r.ci;
            // A pull request keeps its own title and description.
            if (b.kind == Kind::PullRequest) continue;
            if (b.title.empty()) b.title = r.title;
            if (b.notes.empty()) b.notes = r.message;
        }
    }
}

Subject parse_subject(const std::string& title)
{
    Subject s;
    std::string t = title;
    // " (#143)" at the end: the pull request it was merged from.
    const size_t open = t.rfind(" (#");
    if (open != std::string::npos && t.back() == ')') {
        const std::string digits = t.substr(open + 3, t.size() - open - 4);
        if (!digits.empty() && digits.size() <= 7 &&
            std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) {
            s.pr = *number::parse<int>(digits);   // 7 digits at most, checked above
            t.resize(open);
        }
    }
    // "type(scope)!: text": a lower-case word, an optional scope, an optional "!".
    const size_t colon = t.find(": ");
    size_t k = 0;
    while (k < t.size() && k < 16 && std::islower((unsigned char)t[k])) k++;
    if (colon != std::string::npos && k > 0) {
        size_t at = k;
        std::string scope;
        if (at < colon && t[at] == '(') {
            const size_t close = t.find(')', at);
            if (close != std::string::npos && close < colon && close - at - 1 <= 30) {
                scope = t.substr(at + 1, close - at - 1);
                at = close + 1;
            }
        }
        const bool bang = at < colon && t[at] == '!';
        if (bang) at++;
        if (at == colon) {
            s.type = t.substr(0, k);
            s.scope = scope;
            s.breaking = bang;
            t = t.substr(colon + 2);
        }
    }
    s.text = t;
    return s;
}

bool parse_commit(const std::string& text, std::string* sha)
{
    return json_util::parse_with(text, [sha](const json& j) {
        if (!j.is_object()) return false;
        const std::string s = json_util::string_at(j, "sha");
        if (s.size() != 40 || !is_hex(s)) return false;
        *sha = text::lower_ascii(s);
        return true;
    });
}

bool parse_query(const std::string& text, Query* out)
{
    std::string t = text::trim_blank(text);
    if (t.empty()) return false;
    Query q;
    const bool hash = t[0] == '#';
    if (hash) t = t.substr(1);
    const bool digits = !t.empty() && std::all_of(t.begin(), t.end(), [](char c) { return c >= '0' && c <= '9'; });
    // A number is a pull request (up to 6 digits); 7 hex digits or more a commit.
    if (digits && t.size() <= 6) {
        q.pr = *number::parse<int>(t);   // 6 digits at most
        if (q.pr <= 0) return false;
    } else if (!hash && t.size() >= 7 && t.size() <= 40 && is_hex(t)) {
        q.commit = text::lower_ascii(t);
    } else {
        return false;
    }
    *out = q;
    return true;
}

bool newest_build(const std::vector<Artifact>& artifacts, Kind kind, Build* out)
{
    const Artifact* best = nullptr;
    for (const Artifact& a : artifacts)
        if (!best || a.date > best->date) best = &a;
    if (!best) return false;
    Build b;
    b.kind = kind;
    b.artifact = true;
    b.commit = best->commit;
    b.branch = best->branch;
    b.date = best->date;
    b.url = best->url;
    b.size = best->size;
    b.sha256 = best->sha256;
    b.expires = best->expires;
    b.run = best->run;
    *out = b;
    return true;
}

std::vector<Build> combine(const std::vector<Artifact>& artifacts, const std::vector<Pull>& pulls, size_t keep_main)
{
    std::vector<Artifact> sorted = artifacts;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Artifact& a, const Artifact& b) { return a.date > b.date; });
    auto build = [](const Artifact& a, Kind kind) {
        Build b;
        b.kind = kind;
        b.artifact = true;
        b.commit = a.commit;
        b.date = a.date;
        b.url = a.url;
        b.size = a.size;
        b.sha256 = a.sha256;
        b.expires = a.expires;
        b.run = a.run;
        return b;
    };
    std::vector<Build> out;
    std::set<std::string> seen;
    for (const Artifact& a : sorted) {
        if (out.size() >= keep_main) break;
        // Pushed to this repository's main (a fork's "main" is a pull request's branch).
        if (a.branch == "main" && a.head_repo == a.repo && seen.insert(a.commit).second) out.push_back(build(a, Kind::Main));
    }
    std::vector<Pull> by_number = pulls;
    std::sort(by_number.begin(), by_number.end(), [](const Pull& a, const Pull& b) { return a.number > b.number; });
    for (const Pull& p : by_number) {
        // Only branches of this repository: a fork's build runs code nobody
        // with write access has reviewed yet, and installing it replaces
        // the app itself.
        for (const Artifact& a : sorted) {
            if (a.head_repo != a.repo) continue;
            if (a.branch != p.branch || a.head_repo != p.head_repo) continue;
            Build b = build(a, Kind::PullRequest);
            b.pr = p.number;
            b.title = p.title;
            b.notes = p.body;
            b.page = p.page;
            out.push_back(b);
            break;   // the newest only
        }
    }
    return out;
}

Order order_named(const std::string& name)
{
    return name == "number" ? Order::Number : Order::Newest;
}

const char* order_name(Order order)
{
    return order == Order::Number ? "number" : "newest";
}

std::vector<Build> of_kind(const std::vector<Build>& all, Kind kind, Order order)
{
    std::vector<Build> out;
    for (const Build& b : all)
        if (b.kind == kind) out.push_back(b);
    if (order == Order::Number && kind == Kind::PullRequest)
        std::stable_sort(out.begin(), out.end(), [](const Build& a, const Build& b) { return a.pr > b.pr; });
    else   // ISO 8601 dates in UTC sort as text
        std::stable_sort(out.begin(), out.end(), [](const Build& a, const Build& b) { return a.date > b.date; });
    return out;
}

std::string short_date(const std::string& iso)
{
    if (iso.size() < 16) return iso;
    return iso.substr(0, 10) + " " + iso.substr(11, 5);
}

Age age_of(const std::string& iso, int64_t now)
{
    Age a;
    int64_t then = 0;
    if (!date::iso_seconds(iso, &then)) return a;
    const int64_t age = now - then;
    if (age < 60) a.unit = Age::Now;
    else if (age < 3600) a.unit = Age::Minutes, a.count = (int)(age / 60);
    else if (age < 2 * 86400) a.unit = Age::Hours, a.count = (int)(age / 3600);
    else a.unit = Age::Days, a.count = (int)(age / 86400);
    return a;
}

int days_left(const std::string& iso, int64_t now)
{
    int64_t then = 0;
    if (!date::iso_seconds(iso, &then)) return 0;
    return (int)date::days_between(now, then);
}

bool newer(const Build& b, const std::string& running_version, const Build* running)
{
    if (b.kind == Kind::Release) return !running_version.empty() && update::compare_versions(b.version, running_version) > 0;
    return running && b.artifact && b.commit != running->commit && b.date > running->date;
}

namespace
{
const char* ci_name(Ci c)
{
    switch (c) {
        case Ci::Running: return "running";
        case Ci::Passed: return "passed";
        case Ci::Failed: return "failed";
        case Ci::Unknown: break;
    }
    return "";
}

Ci ci_named(const std::string& n)
{
    return n == "running" ? Ci::Running : n == "passed" ? Ci::Passed : n == "failed" ? Ci::Failed : Ci::Unknown;
}

const char* kind_name(Kind k)
{
    switch (k) {
        case Kind::Release: return "release";
        case Kind::Main: return "main";
        case Kind::PullRequest: return "pr";
        case Kind::Commit: return "commit";
    }
    return "";
}
}   // namespace

std::string encode_cache(const Cache& cache)
{
    json list = json::array();
    for (const Build& b : cache.builds)
        list.push_back({ { "kind", kind_name(b.kind) }, { "artifact", b.artifact }, { "version", b.version },
                         { "prerelease", b.prerelease },
                         { "pr", b.pr }, { "title", b.title }, { "commit", b.commit }, { "date", b.date },
                         { "url", b.url }, { "size", b.size }, { "sha256", b.sha256 }, { "notes", b.notes },
                         { "branch", b.branch }, { "expires", b.expires }, { "run", b.run }, { "ci", ci_name(b.ci) },
                         { "page", b.page } });
    const json j = { { "schema", CACHE_SCHEMA }, { "fetched_at", cache.fetched_at }, { "needs_login", cache.needs_login },
                     { "builds", list } };
    return j.dump(1) + "\n";
}

bool decode_cache(const std::string& text, Cache* out)
{
    return json_util::parse_with(text, [out](const json& j) {
        const json* builds = json_util::array_at(j, "builds");
        // "version": the schema's name in the files of earlier builds. Their
        // list is fetched again and written with "schema", so this reading
        // can go once each app has released a build that writes it.
        const int64_t schema = j.is_object() ? json_util::integer_or(j, "schema", json_util::integer_or(j, "version")) : 0;
        if (schema != CACHE_SCHEMA || !builds) return false;
        Cache c;
        c.fetched_at = json_util::integer_or(j, "fetched_at");
        c.needs_login = j.value("needs_login", true);
        for (const auto& x : *builds) {
            if (!x.is_object()) continue;
            Build b;
            const std::string kind = json_util::string_at(x, "kind");
            if (kind == "release") b.kind = Kind::Release;
            else if (kind == "main") b.kind = Kind::Main;
            else if (kind == "pr") b.kind = Kind::PullRequest;
            else if (kind == "commit") b.kind = Kind::Commit;
            else continue;
            b.artifact = x.value("artifact", false);
            // An edited list is no more trusted than GitHub: the same cleaning.
            b.version = json_util::string_at(x, "version");
            b.prerelease = b.kind == Kind::Release && x.value("prerelease", false);
            b.pr = (int)json_util::integer_or(x, "pr");
            b.title = line_text(json_util::string_at(x, "title"));
            b.commit = json_util::string_at(x, "commit");
            b.date = iso_time(json_util::string_at(x, "date"));
            b.url = json_util::string_at(x, "url");
            b.sha256 = json_util::string_at(x, "sha256");
            b.notes = cut_notes(json_util::string_at(x, "notes"));
            b.branch = line_text(json_util::string_at(x, "branch"));
            b.page = page_url(json_util::string_at(x, "page"));
            b.expires = iso_time(json_util::string_at(x, "expires"));
            b.run = json_util::integer_or(x, "run");
            b.ci = ci_named(json_util::string_at(x, "ci"));
            const int64_t size = json_util::integer_or(x, "size");
            const bool commit_ok = b.kind == Kind::Release ? b.commit.empty() : short_commit(b.commit) == b.commit;
            // A build without its digest could not be installed: not kept either.
            const bool sha_ok = b.sha256.size() == 64 && is_hex(b.sha256) && text::lower_ascii(b.sha256) == b.sha256;
            if (!github_url(b.url) || size <= 0 || !commit_ok || !sha_ok) continue;
            if (b.kind == Kind::Release ? !safe_text::is_version(b.version) : !b.artifact) continue;
            if (b.kind == Kind::PullRequest && b.pr <= 0) continue;
            b.size = (uint64_t)size;
            c.builds.push_back(b);
        }
        *out = c;
        return true;
    });
}

bool cache_fresh(const Cache& cache, int64_t now, bool needs_login)
{
    const int64_t age = now - cache.fetched_at;
    return cache.fetched_at > 0 && age >= 0 && age < CACHE_MAX_AGE_S && cache.needs_login == needs_login &&
           !cache.builds.empty();
}

std::string digest_hex(const std::string& digest)
{
    static const std::string PREFIX = "sha256:";
    if (digest.size() != PREFIX.size() + 64 || !text::starts_with(digest, PREFIX.c_str())) return "";
    const std::string hex = digest.substr(PREFIX.size());
    return is_hex(hex) ? text::lower_ascii(hex) : "";
}

bool is_nro(const std::string& head)
{
    return head.size() >= 0x14 && head.compare(0x10, 4, "NRO0") == 0;
}

const char* const NO_DIGEST = "GitHub gives no SHA-256 for this file: not installed";

Done verify_file(const std::string& path, uint64_t size, const std::string& sha256)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return std::unexpected("cannot read the download");
    Sha256 hash;
    uint64_t got = 0;
    char buf[16 * 1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        hash.update(std::string_view(buf, n));
        got += n;
    }
    const bool read_error = std::ferror(f) != 0;
    std::fclose(f);
    if (read_error) return std::unexpected("cannot read the download");
    if (got != size) return std::unexpected("size " + std::to_string(got) + " instead of " + std::to_string(size));
    if (sha256.empty()) return std::unexpected(NO_DIGEST);
    if (hash.hex() != sha256) return std::unexpected("SHA-256 does not match");
    return {};
}

Done verify_nro(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    char head[0x20] = {};
    const size_t n = f ? std::fread(head, 1, sizeof(head), f) : 0;
    if (f) std::fclose(f);
    if (!is_nro(std::string(head, n))) return std::unexpected("not an NRO file");
    return {};
}

Done replace(const std::string& target, const std::string& fresh, const std::string& keep_as)
{
    std::string error;
    if (!paths::replace_file(target, fresh, keep_as, &error)) return std::unexpected(error);
    return {};
}

}   // namespace dev_builds
