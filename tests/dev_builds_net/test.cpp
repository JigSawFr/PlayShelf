// Host tests for dev_builds::download (source/util/dev_builds_net.cpp): with
// PLAYSHELF_SIM_DEV_BUILDS unset, the console's path, against a fake network
// below:
// http::download writes what the test scripted into the file it is given and
// answers as told, http::redirect hands an artifact's signed link. The real
// verify_file, verify_nro and zip_read stand behind it. Covered: the refusals
// before anything is fetched (too large, no digest); a fresh download; a
// <file>.part resumed when its <file>.part.json names the same build, started
// over when it names another one, or when the part is empty or already as
// large as the build; an answer of HTTP 416 to the resume, asked again from
// the start (not when cancelled); a failure keeping what arrived for the next
// attempt, and dropping both files when nothing did; a download that does
// not check out (size, SHA-256, NRO header), nothing left behind; a target
// an attempt cut short left (replaced), and one that cannot be replaced (the
// rename fails); an artifact's zip, its link asked with the token on the API
// only, a refused one (401 forgets the token, 401 and 403 say to sign in
// again), playshelf.nro taken out and the zip removed.
#include "check.h"
#include "fs.hpp"
#include "temp_dir.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "app.hpp"
#include "util/dev_builds.hpp"
#include "util/github_auth.hpp"
#include "util/http.hpp"
#include "util/paths.hpp"
#include "util/sha256.hpp"
#include "util/zip_write.hpp"

using dev_builds::Build;

namespace app
{
const char* repo_url() { return "https://github.com/o/r"; }
}   // namespace app

// One scripted answer of http::download: the bytes it writes (appended on a
// resume, from the start otherwise), then success or `error`. A Range
// request answered 416 writes nothing, as libcurl does.
struct Answer
{
    std::string bytes;
    std::string error;   // "" for a success
};

struct DownloadCall
{
    std::string url, path;
    uint64_t    resume_from = 0;
    bool        meta_there = false;   // <path>.json held the build's identity during the call
};

static std::deque<Answer>  s_answers;
static std::vector<DownloadCall>   s_calls;
static std::string         s_identity;   // what <part>.json should hold during a call
static std::string         s_redirect_to;   // "" : http::redirect fails with s_redirect_error
static std::string         s_redirect_error;
static std::vector<std::string> s_redirect_headers;
static std::atomic<bool>*  s_cancel_on_call = nullptr;   // set by the fake download when given

namespace http
{
bool get(const std::string&, std::string*, std::string* error, size_t, long, const Headers&, long*)
{
    if (error) *error = "not in this test";
    return false;
}

bool redirect(const std::string&, std::string* target, std::string* error, const Headers& headers, long* status)
{
    s_redirect_headers = headers;
    if (status) *status = 302;
    if (s_redirect_to.empty()) {
        if (error) *error = s_redirect_error;
        // The status behind an "HTTP <status>" error (http_logic::succeeded).
        if (status) *status = s_redirect_error.compare(0, 5, "HTTP ") == 0 ? std::atol(s_redirect_error.c_str() + 5) : 0;
        return false;
    }
    *target = s_redirect_to;
    return true;
}

bool download(const std::string& url, const std::string& path, std::string* error, size_t,
              long, std::function<void(uint64_t, uint64_t)>, uint64_t resume_from, const std::atomic<bool>*)
{
    DownloadCall c;
    c.url = url;
    c.path = path;
    c.resume_from = resume_from;
    std::string meta;
    c.meta_there = paths::read_file(path + ".json", meta) && meta == s_identity;
    s_calls.push_back(c);
    if (s_cancel_on_call) s_cancel_on_call->store(true);
    if (s_answers.empty()) {
        if (error) *error = "no answer scripted";
        return false;
    }
    const Answer a = s_answers.front();
    s_answers.pop_front();
    if (a.error == "HTTP 416") {
        if (error) *error = a.error;
        return false;
    }
    FILE* f = std::fopen(path.c_str(), resume_from ? "ab" : "wb");
    if (!f) {
        if (error) *error = "cannot open";
        return false;
    }
    std::fwrite(a.bytes.data(), 1, a.bytes.size(), f);
    std::fclose(f);
    if (!a.error.empty()) {
        if (error) *error = a.error;
        return false;
    }
    return true;
}
}   // namespace http

static std::string sha(const std::string& s)
{
    Sha256 h;
    h.update(s.data(), s.size());
    return h.hex();
}

static std::string nro(size_t size)
{
    std::string s(size, 'z');
    for (size_t i = 0; i < size; i++) s[i] = (char)('a' + i % 26);
    s.replace(0x10, 4, "NRO0");
    return s;
}

using fs::content_of;
using fs::exists;

static Build release(const std::string& content)
{
    Build b;
    b.url = "https://github.com/o/r/releases/download/v1.2.0/playshelf.nro";
    b.size = content.size();
    b.sha256 = sha(content);
    return b;
}

static std::string identity(const Build& b)
{
    return b.url + " " + std::to_string(b.size) + " " + b.sha256 + "\n";
}

// A fresh run: no scripted answer, no call, nothing on disk under `dir`.
static void reset(const std::string& target)
{
    s_answers.clear();
    s_calls.clear();
    s_cancel_on_call = nullptr;
    for (const std::string& p : { target, target + ".part", target + ".part.json", target + ".zip",
                                  target + ".zip.part", target + ".zip.part.json" })
        std::remove(p.c_str());
}

static bool clean(const std::string& file)
{
    return !exists(file + ".part") && !exists(file + ".part.json");
}

static void test_refused()
{
    const std::string target = paths::data_dir() + "/refused.nro";
    reset(target);
    std::string err;
    Build b = release(nro(5000));
    b.size = 64 * 1024 * 1024 + 1;
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr) && err == "too large for PlayShelf");
    b = release(nro(5000));
    b.sha256.clear();
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr) && err == dev_builds::NO_DIGEST);
    CHECK(s_calls.empty() && !exists(target) && clean(target));
}

static void test_fresh()
{
    const std::string target = paths::data_dir() + "/fresh.nro";
    const std::string content = nro(5000);
    const Build b = release(content);
    reset(target);
    s_identity = identity(b);
    s_answers = { { content, "" } };
    std::string err;
    CHECK(dev_builds::download(b, target, &err, nullptr, nullptr));
    REQUIRE(s_calls.size() == 1);
    CHECK(s_calls[0].url == b.url && s_calls[0].path == target + ".part" && s_calls[0].resume_from == 0);
    CHECK(s_calls[0].meta_there);   // what the part is of, written before it is fetched
    CHECK(content_of(target) == content && clean(target));

    // A target an attempt cut short left (FAT's rename would not replace it): replaced.
    reset(target);
    CHECK(paths::atomic_write(target, "left by an earlier attempt"));
    s_answers = { { content, "" } };
    CHECK(dev_builds::download(b, target, &err, nullptr, nullptr));
    CHECK(content_of(target) == content && clean(target));
}

static void test_resume()
{
    const std::string target = paths::data_dir() + "/resume.nro";
    const std::string content = nro(5000);
    const Build b = release(content);
    s_identity = identity(b);
    std::string err;

    // The part of the same build: the rest is asked for and appended.
    reset(target);
    CHECK(paths::atomic_write(target + ".part", content.substr(0, 2000)));
    CHECK(paths::atomic_write(target + ".part.json", identity(b)));
    s_answers = { { content.substr(2000), "" } };
    CHECK(dev_builds::download(b, target, &err, nullptr, nullptr));
    REQUIRE(s_calls.size() == 1);
    CHECK(s_calls[0].resume_from == 2000 && s_calls[0].meta_there);
    CHECK(content_of(target) == content && clean(target));

    // The part of another build (another digest, size or link), or with no
    // identity at all: dropped, from the start.
    for (const std::string& kept : { identity(release(nro(4000))), b.url + " 5000 " + std::string(64, 'a') + "\n",
                                     std::string("https://elsewhere/x 5000 ") + b.sha256 + "\n", std::string() }) {
        reset(target);
        CHECK(paths::atomic_write(target + ".part", "junk from another build"));
        if (!kept.empty()) CHECK(paths::atomic_write(target + ".part.json", kept));
        s_answers = { { content, "" } };
        CHECK(dev_builds::download(b, target, &err, nullptr, nullptr));
        REQUIRE(s_calls.size() == 1);
        CHECK(s_calls[0].resume_from == 0 && s_calls[0].meta_there);
        CHECK(content_of(target) == content && clean(target));
    }

    // An empty part, or one as large as the build (or larger): nothing to go
    // on from, from the start.
    for (const std::string& part : { std::string(), content, content + "more" }) {
        reset(target);
        CHECK(paths::atomic_write(target + ".part", part));
        CHECK(paths::atomic_write(target + ".part.json", identity(b)));
        s_answers = { { content, "" } };
        CHECK(dev_builds::download(b, target, &err, nullptr, nullptr));
        REQUIRE(s_calls.size() == 1);
        CHECK(s_calls[0].resume_from == 0);
        CHECK(content_of(target) == content && clean(target));
    }

    // HTTP 416 to the resume (the server's file changed under the same
    // link): asked again from the start, once.
    reset(target);
    CHECK(paths::atomic_write(target + ".part", content.substr(0, 3000)));
    CHECK(paths::atomic_write(target + ".part.json", identity(b)));
    s_answers = { { "", "HTTP 416" }, { content, "" } };
    CHECK(dev_builds::download(b, target, &err, nullptr, nullptr));
    REQUIRE(s_calls.size() == 2);
    CHECK(s_calls[0].resume_from == 3000 && s_calls[1].resume_from == 0);
    CHECK(content_of(target) == content && clean(target));

    // Cancelled meanwhile: not asked again, the part kept for next time.
    reset(target);
    CHECK(paths::atomic_write(target + ".part", content.substr(0, 3000)));
    CHECK(paths::atomic_write(target + ".part.json", identity(b)));
    std::atomic<bool> cancel(false);
    s_cancel_on_call = &cancel;
    s_answers = { { "", "HTTP 416" }, { content, "" } };
    CHECK(!dev_builds::download(b, target, &err, nullptr, &cancel) && err == "HTTP 416");
    CHECK(s_calls.size() == 1);
    CHECK(content_of(target + ".part") == content.substr(0, 3000) && content_of(target + ".part.json") == identity(b));
    CHECK(!exists(target));

    // HTTP 416 to a download from the start is a plain failure.
    reset(target);
    s_answers = { { "", "HTTP 416" }, { content, "" } };
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr) && err == "HTTP 416");
    CHECK(s_calls.size() == 1 && clean(target) && !exists(target));
}

static void test_cut_short()
{
    const std::string target = paths::data_dir() + "/cut.nro";
    const std::string content = nro(5000);
    const Build b = release(content);
    s_identity = identity(b);
    std::string err;

    // Cut after 1200 bytes: they are kept, with what they are of...
    reset(target);
    s_answers = { { content.substr(0, 1200), "timed out" } };
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr) && err == "timed out");
    CHECK(content_of(target + ".part") == content.substr(0, 1200));
    CHECK(content_of(target + ".part.json") == identity(b));
    CHECK(!exists(target));
    // ... and the next attempt goes on from there.
    s_calls.clear();
    s_answers = { { content.substr(1200), "" } };
    CHECK(dev_builds::download(b, target, &err, nullptr, nullptr));
    REQUIRE(s_calls.size() == 1);
    CHECK(s_calls[0].resume_from == 1200);
    CHECK(content_of(target) == content && clean(target));

    // Nothing arrived: nothing kept.
    reset(target);
    s_answers = { { "", "no connection" } };
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr) && err == "no connection");
    CHECK(clean(target) && !exists(target));

    // A resume that fails at once: the part held before is kept as it was.
    reset(target);
    CHECK(paths::atomic_write(target + ".part", content.substr(0, 700)));
    CHECK(paths::atomic_write(target + ".part.json", identity(b)));
    s_answers = { { "", "HTTP 503" } };
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr) && err == "HTTP 503");
    CHECK(content_of(target + ".part") == content.substr(0, 700) && content_of(target + ".part.json") == identity(b));
}

static void test_bad_download()
{
    const std::string target = paths::data_dir() + "/bad.nro";
    const std::string content = nro(5000);
    const Build b = release(content);
    s_identity = identity(b);
    std::string err;

    // Other bytes than announced: refused, the part dropped (a bad one would
    // only fail again), the target never made.
    std::string other = content;
    other[100] = '!';
    reset(target);
    s_answers = { { other, "" } };
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr) && err == "SHA-256 does not match");
    CHECK(clean(target) && !exists(target));

    // Fewer bytes than announced, yet a success from the server.
    reset(target);
    s_answers = { { content.substr(0, 4000), "" } };
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr) && err == "size 4000 instead of 5000");
    CHECK(clean(target) && !exists(target));

    // The right bytes, but no NRO: refused and removed.
    const std::string zip(5000, 'z');
    const Build not_nro = release(zip);
    s_identity = identity(not_nro);
    reset(target);
    s_answers = { { zip, "" } };
    CHECK(!dev_builds::download(not_nro, target, &err, nullptr, nullptr) && err == "not an NRO file");
    CHECK(clean(target) && !exists(target));

    // The target cannot be replaced (here a folder with something in it):
    // the rename fails, the part goes.
    s_identity = identity(b);
    reset(target);
    REQUIRE(mkdir(target.c_str(), 0755) == 0);
    CHECK(paths::atomic_write(target + "/inside", "x"));
    s_answers = { { content, "" } };
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr) && err == "cannot rename the download");
    CHECK(clean(target));
    CHECK(content_of(target + "/inside") == "x");
    std::remove((target + "/inside").c_str());
    std::remove(target.c_str());
}

static void test_artifact()
{
    const std::string target = paths::data_dir() + "/artifact.nro";
    const std::string inner = nro(9000);
    std::tm now = {};
    now.tm_year = 126;
    now.tm_mon = 9;
    now.tm_mday = 17;
    const std::string zip = zip_write::stored(now, { { "playshelf.nro", inner } });
    Build b;
    b.artifact = true;
    b.url = "https://api.github.com/repos/o/r/actions/artifacts/7/zip";
    b.size = zip.size();
    b.sha256 = sha(zip);
    s_identity = identity(b);
    std::string err;
    CHECK(github_auth::save_token("ghu_abc", &err));

    // The API's link, asked with the token, answers with a signed link on
    // another host, fetched without it into <target>.zip.part; playshelf.nro
    // is taken out, the zip goes.
    reset(target);
    s_redirect_to = "https://pipelines.example/signed";
    s_answers = { { zip, "" } };
    CHECK(dev_builds::download(b, target, &err, nullptr, nullptr));
    REQUIRE(s_calls.size() == 1);
    CHECK(s_calls[0].url == s_redirect_to && s_calls[0].path == target + ".zip.part" && s_calls[0].meta_there);
    CHECK(s_redirect_headers == github_auth::api_headers("ghu_abc"));
    CHECK(content_of(target) == inner && !exists(target + ".zip") && clean(target + ".zip"));

    // A list naming another host: the token does not go there.
    Build elsewhere = b;
    elsewhere.url = "https://example.com/artifact.zip";
    s_identity = identity(elsewhere);
    reset(target);
    s_answers = { { zip, "" } };
    CHECK(dev_builds::download(elsewhere, target, &err, nullptr, nullptr));
    CHECK(s_redirect_headers == github_auth::api_headers(""));
    CHECK(content_of(target) == inner);

    // Refused: 403 (a token without the Actions permission) keeps it, 401
    // (revoked) forgets it; both say what to do.
    s_identity = identity(b);
    s_redirect_to.clear();
    reset(target);
    s_redirect_error = "HTTP 403";
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr));
    CHECK(err == "HTTP 403: GitHub refused the download, sign out of GitHub and sign in again");
    CHECK(github_auth::token() == "ghu_abc");
    s_redirect_error = "HTTP 401";
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr));
    CHECK(err == "HTTP 401: GitHub refused the download, sign out of GitHub and sign in again");
    CHECK(github_auth::token().empty());
    s_redirect_error = "HTTP 500";
    CHECK(!dev_builds::download(b, target, &err, nullptr, nullptr) && err == "HTTP 500");
    CHECK(s_calls.empty() && clean(target + ".zip") && !exists(target) && !exists(target + ".zip"));

    // A zip without playshelf.nro: nothing installed, the zip removed.
    const std::string empty_zip = zip_write::stored(now, { { "other.txt", "hello" } });
    Build no_nro = b;
    no_nro.size = empty_zip.size();
    no_nro.sha256 = sha(empty_zip);
    s_identity = identity(no_nro);
    s_redirect_to = "https://pipelines.example/signed";
    reset(target);
    s_answers = { { empty_zip, "" } };
    CHECK(!dev_builds::download(no_nro, target, &err, nullptr, nullptr));
    CHECK(!exists(target) && !exists(target + ".zip") && clean(target + ".zip"));
}

int main()
{
    TempDir tmp("dev_builds_net", TempDir::Enter);   // paths::data_dir() is ./playshelf_data on the host
    REQUIRE(paths::ensure_dir(paths::data_dir()));
    unsetenv("PLAYSHELF_SIM_DEV_BUILDS");   // the network, not a simulated folder

    test_refused();
    test_fresh();
    test_resume();
    test_cut_short();
    test_bad_download();
    test_artifact();

    tmp.remove();
    return CHECK_DONE("dev_builds_net tests passed");
}
