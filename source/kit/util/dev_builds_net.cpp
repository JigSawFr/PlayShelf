// The network half of dev_builds.hpp (the rest stays plain C++ for the host tests).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef __SWITCH__
#include <chrono>
#include <thread>

#include "sim/sim_net.hpp"
#endif

#include <aurore/cadran/util/text.hpp>

#include "app.hpp"
#include "app_id.h"
#include "core/sim_env.h"
#include "util/dev_builds.hpp"
#include "util/github_auth.hpp"
#include "util/http.hpp"
#include "util/http_logic.hpp"
#include "util/paths.hpp"
#include "util/zip_read.hpp"

namespace dev_builds
{

namespace
{
// A .nro is about 5 MB, its artifact a little less; anything much larger is not one.
constexpr uint64_t MAX_DOWNLOAD = 64 * 1024 * 1024;

// "https://github.com/o/r" -> "https://api.github.com/repos/o/r"
std::string api_url()
{
    const std::string repo = app::repo_url();
    const std::string host = "https://github.com/";
    return github_auth::api_url("repos/" + repo.substr(host.size()));
}

#ifndef __SWITCH__
const char* sim()
{
    return sim_net::value(APP_ENV "_SIM_DEV_BUILDS");
}
#endif

// GET `path` of the API (or, simulated, the file `name` of the folder).
bool api_get(const std::string& path, const char* name, const std::string& token, std::string* body,
             std::string* error, size_t max)
{
#ifndef __SWITCH__
    if (const char* folder = sim()) {
        // <APP_ENV>_SIM_DEV_BUILDS_DELAY: each answer takes that many ms (the bar shows).
        if (const long long delay = sim_env_int(APP_ENV "_SIM_DEV_BUILDS_DELAY", 0, 0, 60000))
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        return sim_net::get(folder, name, body, error);
    }
#endif
    (void)name;
    auto got = http::get(api_url() + path, max, 30, github_auth::api_headers(token));
    if (!got) {
        // A token GitHub no longer takes (revoked): forgotten, the next fetch
        // lists what needs none and asks to sign in again.
        github_auth::forget_if_revoked(got.error().status, token);
        if (error) *error = got.error().error;
        return false;
    }
    *body = std::move(got->body);
    return true;
}
}   // namespace

std::expected<std::vector<Build>, std::string> fetch(bool* needs_login, Progress progress)
{
    std::vector<Build> builds_found;
    std::vector<Build>* const out = &builds_found;
    const std::string token = github_auth::token();
    *needs_login = token.empty();
    std::string body, err;
    bool reached = false;
    // Signed out, the releases are all there is to fetch.
    auto step = [&progress, &token](float done, Step s) {
        if (progress) progress(token.empty() ? (done > 0 ? 1.0f : 0.0f) : done, s);
    };
    step(0.0f, Step::Releases);

    // The last releases, newest first (each one's notes come along: a few
    // KB apiece).
    if (api_get("/releases?per_page=" + std::to_string(MAX_RELEASES), "releases.json", token, &body, &err,
                4 * 1024 * 1024)) {
        reached = true;
        parse_releases(body, out);
    }
    if (!token.empty()) {
        // The artifacts of the last runs, newest first, 100 a page: the API
        // only filters by an exact name, and a run names its artifacts after
        // its build (is_build_artifact keeps the one holding <app>.nro,
        // about one in six). Five pages are some eighty runs, more than
        // main's last commits and the open pull requests need; a page with
        // fewer than 100 entries is the last one (the simulated folder answers
        // every page with the same file, which is short, so one page). The
        // open pull requests name the branches.
        std::vector<Artifact> artifacts;
        std::vector<Pull> pulls;
        bool a = false;
        // The share of each step: the artifacts' pages take most of it.
        for (int page = 1; page <= 5; page++) {
            step(0.15f + 0.1f * (page - 1), Step::Artifacts);
            size_t listed = 0;
            if (!api_get("/actions/artifacts?per_page=100&page=" + std::to_string(page), "artifacts.json", token,
                         &body, &err, 4 * 1024 * 1024) ||
                !parse_artifacts(body, &artifacts, &listed))
                break;
            a = true;
            if (listed < 100) break;
        }
        step(0.65f, Step::Pulls);
        const bool p = api_get("/pulls?state=open&per_page=50", "pulls.json", token, &body, &err, 4 * 1024 * 1024) &&
                       parse_pulls(body, &pulls);
        reached |= a || p;
        std::vector<Build> builds = combine(artifacts, pulls);
        // How the runs that made them ended: the build workflow's last 100
        // runs (an older one stays unknown). Nice to have: no answer, no state.
        step(0.85f, Step::Runs);
        std::vector<Run> runs;
        if (api_get("/actions/workflows/build.yml/runs?per_page=100", "runs.json", token, &body, &err, 8 * 1024 * 1024) &&
            parse_runs(body, &runs))
            apply_runs(&builds, runs);
        for (Build& b : builds) out->push_back(std::move(b));
    }
    step(1.0f, Step::Runs);
    if (!out->empty()) return builds_found;
    return std::unexpected(reached ? std::string("no build found") : err);
}

std::expected<Build, std::string> find(const Query& q)
{
    const std::string token = github_auth::token();
    std::string body, err;
    auto fail = [](const std::string& why) { return std::unexpected(why); };
    if (token.empty()) return fail("sign in to GitHub first");
    Pull pull;
    std::string sha;
    if (q.pr) {
        if (!api_get("/pulls/" + std::to_string(q.pr), ("pull_" + std::to_string(q.pr) + ".json").c_str(), token, &body,
                     &err, 4 * 1024 * 1024))
            return fail(err == "HTTP 404" ? "no pull request #" + std::to_string(q.pr) : err);
        if (!parse_pull(body, &pull)) return fail("unexpected answer from GitHub");
        // Never a fork's build: nobody with write access has reviewed its code.
        if (pull.head_repo != pull.repo) return fail("pull request from a fork");
        sha = pull.head_sha;
    } else {
        if (!api_get("/commits/" + q.commit, "commit.json", token, &body, &err, 4 * 1024 * 1024))
            return fail(err == "HTTP 404" || err == "HTTP 422" ? "no commit " + q.commit : err);
        if (!parse_commit(body, &sha)) return fail("unexpected answer from GitHub");
    }
    // The build workflow's runs of that commit, then the newest one's artifact.
    std::vector<Run> runs;
    if (!api_get("/actions/workflows/build.yml/runs?per_page=20&head_sha=" + sha, "runs.json", token, &body, &err,
                 4 * 1024 * 1024) ||
        !parse_runs(body, &runs))
        return fail(err.empty() ? "unexpected answer from GitHub" : err);
    for (const Run& r : runs) {
        if (r.sha != sha) continue;   // the simulated folder answers any commit
        if (r.head_repo != r.repo) continue;   // a fork's run of the same commit
        std::vector<Artifact> artifacts;
        if (!api_get("/actions/runs/" + std::to_string(r.id) + "/artifacts?per_page=100", "run_artifacts.json", token,
                     &body, &err, 4 * 1024 * 1024) ||
            !parse_artifacts(body, &artifacts))
            continue;
        Build b;
        if (!newest_build(artifacts, q.pr ? Kind::PullRequest : Kind::Commit, &b)) continue;
        b.ci = r.ci;
        b.title = r.title;
        b.notes = r.message;
        if (q.pr) {
            b.pr = pull.number;
            b.title = pull.title;
            b.notes = pull.body;
            b.page = pull.page;
        }
        return b;
    }
    return fail("no build of " + (q.pr ? "#" + std::to_string(q.pr) : sha.substr(0, 7)) + " left (artifacts expire after 90 days)");
}

std::expected<Build, std::string> fetch_release()
{
    std::string body, err;
    if (!api_get("/releases/latest", "latest.json", github_auth::token(), &body, &err, 512 * 1024)) return std::unexpected(err);
    Build b;
    if (!parse_release(body, &b)) return std::unexpected("no " APP_ID ".nro in the latest release");
    return b;
}

Done download(const Build& b, const std::string& path, std::function<void(uint64_t done, uint64_t total)> progress,
              const std::atomic<bool>* cancel)
{
    if (b.size > MAX_DOWNLOAD) return std::unexpected("too large for " APP_NAME);
    // Refused before anything is fetched (verify_file would, after).
    if (b.sha256.empty()) return std::unexpected(NO_DIGEST);
    // Why it stopped, written by the steps below.
    std::string why;
    std::string* const error = &why;
    // An artifact arrives as a zip next to the target, <app>.nro is taken out of it.
    const std::string file = b.artifact ? path + ".zip" : path;
    // Received into <file>.part; a download cut short leaves it, with what it
    // is of (<file>.part.json), and the next one of the same build goes on
    // from there.
    const std::string part = file + ".part", meta = part + ".json";
    const std::string identity = b.url + " " + std::to_string(b.size) + " " + b.sha256 + "\n";
    uint64_t resume_from = 0;
    {
        std::string kept;
        FILE* f = std::fopen(part.c_str(), "rb");
        if (f && paths::read_file(meta, kept) && kept == identity && std::fseek(f, 0, SEEK_END) == 0) {
            const long have = std::ftell(f);
            if (have > 0 && (uint64_t)have < b.size) resume_from = (uint64_t)have;
        }
        if (f) std::fclose(f);
        if (!resume_from) std::remove(part.c_str());
        std::string werr;
        if (!paths::atomic_write(meta, identity, &werr)) std::remove(meta.c_str());   // only loses the resume
    }
    bool ok;
#ifndef __SWITCH__
    if (sim()) {
        std::string content;
        ok = text::starts_with(b.url, "https://") && paths::read_file(b.url.substr(8), content);
        // A few steps, slow enough for the progress bar to be seen, and
        // for the smoke test to cancel one.
        for (int i = 1; ok && progress && i <= 8; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (cancel && cancel->load()) {
                if (error) *error = "cancelled";
                ok = false;
                break;
            }
            progress(content.size() * i / 8, content.size());
        }
        if (ok) {
            FILE* f = std::fopen(part.c_str(), resume_from ? "ab" : "wb");
            const std::string rest = resume_from < content.size() ? content.substr(resume_from) : "";
            ok = f && std::fwrite(rest.data(), 1, rest.size(), f) == rest.size();
            if (f && std::fclose(f) != 0) ok = false;
        }
        if (!ok && error && error->empty()) *error = "simulated: cannot read " + b.url;
    } else
#endif
    {
        std::string link = b.url;
        ok = true;
        if (b.artifact) {
            // The API answers with a short-lived signed link to the zip, on
            // another host: fetched without the token (asked again for a
            // resumed download, the last one has expired).
            // The token only ever goes to the API itself, whatever the list says.
            const bool api = text::starts_with(b.url, github_auth::API_ROOT);
            const std::string token = api ? github_auth::token() : "";
            const auto target = http::redirect(b.url, github_auth::api_headers(token));
            ok = target.has_value();
            if (ok) {
                link = *target;
            } else {
                // A token without the Actions permission (an older sign-in) lists the
                // builds but is refused the download (403); a revoked one is
                // refused everything (401), and forgotten.
                const long status = target.error().status;
                github_auth::forget_if_revoked(status, token);
                if (error) {
                    *error = target.error().error;
                    if (status == 401 || status == 403) *error += ": GitHub refused the download, sign out of GitHub and sign in again";
                }
            }
        }
        const auto fetch_part = [&](uint64_t from) {
            const auto done = http::download(link, part, (size_t)b.size + 1, 600, progress, from, cancel);
            if (!done && error) *error = done.error().error;
            return done.has_value();
        };
        if (ok) ok = fetch_part(resume_from);
        // The part kept no longer matches what the server has (416), or the
        // server cannot resume it (libcurl stops before the 200 that starts
        // over is written): from the start, or every later attempt would stop
        // the same way.
        if (!ok && resume_from && error && http_logic::restart_resume(*error) && !(cancel && cancel->load())) {
            resume_from = 0;
            std::remove(part.c_str());
            ok = fetch_part(0);
        }
    }
    if (!ok) {
        // Cut short: what arrived is kept for the next attempt (nothing: nothing kept).
        FILE* f = std::fopen(part.c_str(), "rb");
        const bool some = f && std::fgetc(f) != EOF;
        if (f) std::fclose(f);
        if (!some) {
            std::remove(part.c_str());
            std::remove(meta.c_str());
        }
        return std::unexpected(why);
    }
    // Complete: checked whole, the part kept no more (a bad one would only fail again).
    if (const auto checked = verify_file(part, b.size, b.sha256); !checked) {
        why = checked.error();
        ok = false;
    }
    std::remove(meta.c_str());
    if (ok) std::remove(file.c_str());   // left by an attempt cut short: FAT rename does not replace it
    if (ok && std::rename(part.c_str(), file.c_str()) != 0) {
        if (error) *error = "cannot rename the download";
        ok = false;
    }
    std::remove(part.c_str());
    if (ok && b.artifact) ok = zip_read::extract(file, APP_ID ".nro", path, MAX_DOWNLOAD, error);
    if (ok) {
        if (const auto nro = verify_nro(path); !nro) {
            why = nro.error();
            ok = false;
        }
    }
    if (b.artifact) std::remove(file.c_str());
    if (!ok) {
        std::remove(path.c_str());
        return std::unexpected(why);
    }
    return {};
}

}   // namespace dev_builds
