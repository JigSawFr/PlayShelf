// Host tests for source/util/sha256.cpp (and its HMAC), dev_builds.cpp, zip_read.cpp and
// github_auth.cpp: the SHA-256 test vectors, digests, the latest release and
// the list of releases, the
// artifact and pull-request lists and how they combine (main's commits, each
// open pull request's newest build, forks included), the cached list, the download checks
// (size, SHA-256, NRO header), zip extraction (deflated, stored, a changed
// byte, a cut archive), the replacement of the running file, and the
// device-flow answers and the token file.
#include "check.h"
#include "fs.hpp"
#include "temp_dir.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "util/date.hpp"
#include "util/dev_builds.hpp"
#include "util/paths.hpp"
#include "util/github_auth.hpp"
#include "util/sha256.hpp"
#include "util/zip_read.hpp"

using dev_builds::Build;
using dev_builds::Kind;

static std::string sha(const std::string& s)
{
    Sha256 h;
    h.update(s.data(), s.size());
    return h.hex();
}

static void test_sha256()
{
    CHECK(sha("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
           "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // A million 'a', fed in uneven pieces: crosses many block boundaries.
    Sha256 h;
    const std::string chunk(997, 'a');
    size_t left = 1000000;
    while (left > 0) {
        const size_t n = std::min(left, chunk.size());
        h.update(chunk.data(), n);
        left -= n;
    }
    CHECK(h.hex() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    // 55, 56 and 64 bytes: the padding edge cases.
    CHECK(sha(std::string(55, 'x')).size() == 64);
    CHECK(sha(std::string(56, 'a')) == "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    CHECK(sha(std::string(64, 'a')) == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
}

static std::string hex_of(const std::array<uint8_t, 32>& d)
{
    static const char HEX[] = "0123456789abcdef";
    std::string out;
    for (uint8_t b : d) {
        out += HEX[b >> 4];
        out += HEX[b & 0xF];
    }
    return out;
}

// RFC 4231's test cases 1, 2 and 6 (a key longer than a block).
static void test_hmac_sha256()
{
    const std::string k1(20, '\x0b'), m1 = "Hi There";
    CHECK(hex_of(hmac_sha256(k1.data(), k1.size(), m1.data(), m1.size())) ==
          "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    const std::string k2 = "Jefe", m2 = "what do ya want for nothing?";
    CHECK(hex_of(hmac_sha256(k2.data(), k2.size(), m2.data(), m2.size())) ==
          "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    const std::string k6(131, '\xaa'), m6 = "Test Using Larger Than Block-Size Key - Hash Key First";
    CHECK(hex_of(hmac_sha256(k6.data(), k6.size(), m6.data(), m6.size())) ==
          "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    Sha256 h;
    CHECK(hex_of(h.digest()) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

static void test_names()
{
    const std::string hex(64, 'a');
    CHECK(dev_builds::digest_hex("sha256:" + hex) == hex);
    CHECK(dev_builds::digest_hex("sha256:" + std::string(64, 'A')) == hex);
    CHECK(dev_builds::digest_hex("sha512:" + hex).empty());
    CHECK(dev_builds::digest_hex("sha256:" + hex.substr(1)).empty());
    CHECK(dev_builds::digest_hex("sha256:" + hex.substr(1) + "z").empty());
    CHECK(dev_builds::digest_hex("").empty());

    std::string head(0x20, '\0');
    CHECK(!dev_builds::is_nro(head));
    head.replace(0x10, 4, "NRO0");
    CHECK(dev_builds::is_nro(head));
    CHECK(!dev_builds::is_nro(head.substr(0, 0x12)));
}

static void test_release()
{
    Build b;
    const std::string rel = R"({"tag_name": "v1.2.0", "prerelease": false, "draft": false,
        "body": "## [1.2.0](x) (2026-10-01)\n\n### Features\n\n* in place", "assets": [
        {"name": "playshelf.zip", "size": 9, "updated_at": "x", "browser_download_url": "https://x/z"},
        {"name": "playshelf.nro", "size": 1000, "updated_at": "2026-10-01T00:00:00Z",
         "browser_download_url": "https://github.com/o/r/releases/download/v1.2.0/playshelf.nro",
         "digest": "sha256:)" + std::string(64, 'c') + R"("}]})";
    CHECK(dev_builds::parse_release(rel, &b));
    CHECK(b.kind == Kind::Release && !b.artifact && b.version == "1.2.0" && b.size == 1000 &&
           b.sha256 == std::string(64, 'c') && b.url.find("playshelf.nro") != std::string::npos);
    CHECK(b.notes == "## [1.2.0](x) (2026-10-01)\n\n### Features\n\n* in place");
    // Long notes are cut on a whole line.
    std::string body;
    while (body.size() <= 2 * dev_builds::MAX_NOTES) body += "* a line of notes\\n";
    std::string longer = rel;
    longer.replace(longer.find("## [1.2.0]"), 0, body);
    CHECK(dev_builds::parse_release(longer, &b));
    CHECK(b.notes.size() <= dev_builds::MAX_NOTES && !b.notes.empty() && b.notes.back() == 's');   // "…notes", not half a line
    const char* bad[] = {
        R"({"tag_name": "v1", "prerelease": true, "draft": false, "assets": [{"name": "playshelf.nro", "size": 1, "browser_download_url": "https://x"}]})",
        R"({"tag_name": "v1", "prerelease": false, "draft": true, "assets": [{"name": "playshelf.nro", "size": 1, "browser_download_url": "https://x"}]})",
        R"({"tag_name": "v1", "prerelease": false, "draft": false, "assets": [{"name": "playshelf.zip", "size": 1, "browser_download_url": "https://x"}]})",
        R"({"tag_name": "v1", "prerelease": false, "draft": false, "assets": [{"name": "playshelf.nro", "size": 0, "browser_download_url": "https://x"}]})",
        R"({"tag_name": "v1", "prerelease": false, "draft": false, "assets": [{"name": "playshelf.nro", "size": 1, "browser_download_url": "http://x"}]})",
        R"({"message": "Not Found"})", "[]", "not json", "",
    };
    for (const char* j : bad) CHECK(!dev_builds::parse_release(j, &b));
}

// One release of GET /releases.
static std::string release_json(const std::string& tag, bool prerelease, bool draft, const std::string& asset = "playshelf.nro")
{
    return R"({"tag_name": ")" + tag + R"(", "prerelease": )" + (prerelease ? "true" : "false") +
           R"(, "draft": )" + (draft ? "true" : "false") + R"(, "body": "notes", "assets": [{"name": ")" + asset +
           R"(", "size": 1000, "updated_at": "2026-10-01T00:00:00Z", "browser_download_url": "https://github.com/o/r/releases/download/)" +
           tag + R"(/playshelf.nro"}]})";
}

static void test_releases()
{
    std::vector<Build> out;
    const std::string list = "[" + release_json("v1.3.0-rc.1", true, false) + "," + release_json("v1.2.0", false, false) +
                             "," + release_json("v1.2.1", false, true) + "," +   // a draft
                             release_json("v1.1.0", false, false, "playshelf.zip") + "," +   // no .nro
                             release_json("v1.2.0", false, false) + "," +   // the same version again
                             R"("not a release", {"tag_name": "v0.9.0"},)" + release_json("v1.0.0", false, false) + "]";
    CHECK(dev_builds::parse_releases(list, &out));
    if (!CHECK(out.size() == 3)) return;
    CHECK(out[0].version == "1.3.0-rc.1" && out[0].prerelease && out[0].kind == Kind::Release && !out[0].artifact);
    CHECK(out[1].version == "1.2.0" && !out[1].prerelease && out[1].size == 1000 && out[1].notes == "notes");
    CHECK(out[2].version == "1.0.0");
    // At most `keep`, the newest.
    out.clear();
    CHECK(dev_builds::parse_releases(list, &out, 2) && out.size() == 2 && out[1].version == "1.2.0");
    // Not a list.
    out.clear();
    CHECK(!dev_builds::parse_releases(R"({"message": "Not Found"})", &out) && out.empty());
    CHECK(!dev_builds::parse_releases("not json", &out) && out.empty());
    CHECK(dev_builds::parse_releases("[]", &out) && out.empty());
    // The latest release alone still never takes a pre-release.
    Build b;
    CHECK(!dev_builds::parse_release(release_json("v1.3.0-rc.1", true, false), &b));
    CHECK(dev_builds::parse_release(release_json("v1.2.0", false, false), &b) && !b.prerelease);
}

static void test_find_and_runs()
{
    // What Find a build takes.
    dev_builds::Query q;
    CHECK(dev_builds::parse_query("38", &q) && q.pr == 38 && q.commit.empty());
    CHECK(dev_builds::parse_query(" #145 ", &q) && q.pr == 145);
    CHECK(dev_builds::parse_query("ABCDEF1", &q) && q.pr == 0 && q.commit == "abcdef1");
    CHECK(dev_builds::parse_query("1234567", &q) && q.commit == "1234567");   // 7 digits: a commit
    CHECK(dev_builds::parse_query(std::string(40, 'a'), &q) && q.commit.size() == 40);
    const std::string bad[] = { "", "  ", "#", "0", "#abcdef1", "abcdef", "xyz1234", std::string(41, 'a'), "-3", "3 4" };
    for (const std::string& t : bad) CHECK(!dev_builds::parse_query(t, &q));

    // One pull request: its description cut on a line, the fork told apart.
    dev_builds::Pull p;
    const std::string pull = R"({"number": 38, "title": "feat: x", "body": "## What\nmore",
        "head": {"ref": "feat/x", "sha": ")" + std::string(40, 'c') + R"(", "repo": {"id": 1}},
        "base": {"ref": "main", "repo": {"id": 1}}})";
    CHECK(dev_builds::parse_pull(pull, &p) && p.number == 38 && p.body == "## What\nmore" &&
          p.head_sha == std::string(40, 'c') && p.head_repo == 1 && p.repo == 1);
    std::string fork = pull;
    fork.replace(fork.find(R"("repo": {"id": 1}})"), 18, R"("repo": {"id": 7}})");
    CHECK(dev_builds::parse_pull(fork, &p) && p.head_repo == 7 && p.repo == 1);
    CHECK(!dev_builds::parse_pull(R"({"message": "Not Found"})", &p));

    std::string sha;
    CHECK(dev_builds::parse_commit(R"({"sha": ")" + std::string(40, 'A') + R"("})", &sha) && sha == std::string(40, 'a'));
    CHECK(!dev_builds::parse_commit(R"({"sha": "abc"})", &sha) && !dev_builds::parse_commit("[]", &sha));

    // The runs: passed, failed (or cancelled), still running.
    std::vector<dev_builds::Run> runs;
    const std::string list = R"({"total_count": 4, "workflow_runs": [
        {"id": 11, "head_sha": ")" + std::string(40, 'a') + R"(", "status": "completed", "conclusion": "success",
         "head_branch": "main", "repository": {"id": 1}, "head_repository": {"id": 1}},
        {"id": 12, "head_sha": ")" + std::string(40, 'b') + R"(", "status": "completed", "conclusion": "cancelled"},
        {"id": 13, "head_sha": ")" + std::string(40, 'c') + R"(", "status": "in_progress", "conclusion": null},
        {"id": 14, "head_sha": "nope", "status": "completed", "conclusion": "success"}]})";
    CHECK(dev_builds::parse_runs(list, &runs));
    if (!CHECK(runs.size() == 3)) return;
    CHECK(runs[0].ci == dev_builds::Ci::Passed && runs[0].repo == 1 && runs[0].head_repo == 1 && runs[0].branch == "main");
    CHECK(runs[1].ci == dev_builds::Ci::Failed && runs[2].ci == dev_builds::Ci::Running);
    CHECK(!dev_builds::parse_runs("[]", &runs));
    std::vector<Build> builds(3);
    builds[0].artifact = true, builds[0].run = 12;
    builds[1].artifact = true, builds[1].run = 99;   // an older run, not listed
    builds[2].run = 11;                               // not an artifact
    dev_builds::apply_runs(&builds, runs);
    CHECK(builds[0].ci == dev_builds::Ci::Failed && builds[1].ci == dev_builds::Ci::Unknown &&
          builds[2].ci == dev_builds::Ci::Unknown);

    // A commit of main takes its run's title and message; a pull request keeps its own.
    std::vector<dev_builds::Run> titled(1);
    titled[0].id = 21, titled[0].title = "fix(ui): wrap (#141)", titled[0].message = "fix(ui): wrap (#141)\n\nbody";
    std::vector<Build> two(2);
    two[0].artifact = two[1].artifact = true, two[0].run = two[1].run = 21;
    two[0].kind = Kind::Main;
    two[1].kind = Kind::PullRequest, two[1].title = "PR title";
    dev_builds::apply_runs(&two, titled);
    CHECK(two[0].title == "fix(ui): wrap (#141)" && two[0].notes == titled[0].message);
    CHECK(two[1].title == "PR title" && two[1].notes.empty());
    std::vector<dev_builds::Run> with_message;
    CHECK(dev_builds::parse_runs(R"({"workflow_runs": [{"id": 5, "head_sha": ")" + std::string(40, 'a') +
                                     R"(", "status": "completed", "conclusion": "success", "display_title": "feat: x\u202e",
                                     "head_commit": {"message": "feat: x\n\nlong"}}]})", &with_message) &&
          with_message.size() == 1 && with_message[0].title == "feat: x" && with_message[0].message == "feat: x\n\nlong");

    // Conventional titles.
    dev_builds::Subject t = dev_builds::parse_subject("feat(play-timer)!: show the time over the limit (#140)");
    CHECK(t.type == "feat" && t.scope == "play-timer" && t.breaking && t.pr == 140 && t.text == "show the time over the limit");
    t = dev_builds::parse_subject("docs: a video tour (#142)");
    CHECK(t.type == "docs" && t.scope.empty() && !t.breaking && t.pr == 142 && t.text == "a video tour");
    t = dev_builds::parse_subject("chore(main): release 1.1.0");
    CHECK(t.type == "chore" && t.scope == "main" && t.pr == 0 && t.text == "release 1.1.0");
    t = dev_builds::parse_subject("Merge branch 'x': into y (#abc)");
    CHECK(t.type.empty() && t.pr == 0 && t.text == "Merge branch 'x': into y (#abc)");
    t = dev_builds::parse_subject("Update README.md");
    CHECK(t.type.empty() && t.text == "Update README.md");
    t = dev_builds::parse_subject("fix(: nope");
    CHECK(t.type.empty() && t.text == "fix(: nope");
    CHECK(dev_builds::parse_subject("").text.empty());

    // A run's artifacts: the newest build one.
    std::vector<dev_builds::Artifact> arts(2);
    arts[0].date = "2026-10-01T00:00:00Z", arts[0].commit = "aaaaaaa", arts[0].run = 5, arts[0].branch = "fix/y";
    arts[1].date = "2026-10-02T00:00:00Z", arts[1].commit = "bbbbbbb", arts[1].run = 6, arts[1].branch = "fix/y";
    Build b;
    CHECK(dev_builds::newest_build(arts, Kind::Commit, &b) && b.commit == "bbbbbbb" && b.run == 6 && b.artifact &&
          b.kind == Kind::Commit && b.branch == "fix/y");
    CHECK(!dev_builds::newest_build({}, Kind::Commit, &b));

    // Newer than the running build.
    Build rel;
    rel.kind = Kind::Release;
    rel.version = "1.2.0";
    CHECK(dev_builds::newer(rel, "1.1.0", nullptr) && !dev_builds::newer(rel, "1.2.0", nullptr) &&
          !dev_builds::newer(rel, "1.10.0", nullptr) && !dev_builds::newer(rel, "", nullptr));
    rel.version = "1.2.0-rc.1";
    CHECK(!dev_builds::newer(rel, "1.2.0", nullptr) && dev_builds::newer(rel, "1.1.0", nullptr));
    Build running, later, earlier;
    running.artifact = later.artifact = earlier.artifact = true;
    running.commit = "aaaaaaa", running.date = "2026-10-05T00:00:00Z";
    later.commit = "bbbbbbb", later.date = "2026-10-06T00:00:00Z";
    earlier.commit = "ccccccc", earlier.date = "2026-10-04T00:00:00Z";
    CHECK(dev_builds::newer(later, "", &running) && !dev_builds::newer(earlier, "", &running) &&
          !dev_builds::newer(running, "", &running) && !dev_builds::newer(later, "", nullptr));

    // Whole days left before a timestamp (an artifact's expiry).
    int64_t now = 0;
    CHECK(date::iso_seconds("2026-10-01T12:00:00Z", &now));
    CHECK(dev_builds::days_left("2026-10-04T11:59:59Z", now) == 2 && dev_builds::days_left("2026-10-04T12:00:00Z", now) == 3);
    CHECK(dev_builds::days_left("2026-10-01T11:00:00Z", now) == -1 && dev_builds::days_left("2026-10-01T12:00:00Z", now) == 0);
    CHECK(dev_builds::days_left("x", now) == 0 && dev_builds::days_left("2026-13-01T00:00:00Z", now) == 0);

    // A build's date shorter, and its age.
    CHECK(dev_builds::short_date("2026-10-09T12:03:00Z") == "2026-10-09 12:03");
    CHECK(dev_builds::short_date("2026-10-09T12:0") == "2026-10-09T12:0" && dev_builds::short_date("").empty());
    auto age = [now](const char* iso, dev_builds::Age::Unit unit, int count) {
        const dev_builds::Age a = dev_builds::age_of(iso, now);
        return a.unit == unit && a.count == count;
    };
    CHECK(age("2026-10-01T11:59:01Z", dev_builds::Age::Now, 0) && age("2026-10-02T00:00:00Z", dev_builds::Age::Now, 0));   // a time to come
    CHECK(age("2026-10-01T11:59:00Z", dev_builds::Age::Minutes, 1) && age("2026-10-01T11:00:01Z", dev_builds::Age::Minutes, 59));
    CHECK(age("2026-10-01T11:00:00Z", dev_builds::Age::Hours, 1) && age("2026-09-29T12:00:01Z", dev_builds::Age::Hours, 47));
    CHECK(age("2026-09-29T12:00:00Z", dev_builds::Age::Days, 2) && age("2025-10-01T12:00:00Z", dev_builds::Age::Days, 365));
    CHECK(age("x", dev_builds::Age::None, 0) && age("2026-10-01 12:00:00Z", dev_builds::Age::None, 0));
}

// The artifact holding playshelf.nro, named after its build as the workflow
// does (`name` for another one).
static std::string artifact(const std::string& branch, const std::string& sha, const std::string& date,
                            int64_t head_repo = 1, bool expired = false, const std::string& name = "")
{
    const std::string n = name.empty() ? "playshelf-1.0.0-" + std::string(branch == "main" ? "main" : "pr41") + "-" +
                                             sha.substr(0, 7) + "-nro"
                                       : name;
    return R"({"name": ")" + n + R"(", "size_in_bytes": 4000, "expired": )" + (expired ? "true" : "false") +
           R"(, "created_at": ")" + date + R"(", "digest": "sha256:)" + std::string(64, 'd') +
           R"(", "archive_download_url": "https://api.github.com/repos/o/r/actions/artifacts/)" + sha.substr(0, 3) +
           R"(/zip", "workflow_run": {"id": 9, "repository_id": 1, "head_repository_id": )" +
           std::to_string(head_repo) + R"(, "head_branch": ")" + branch + R"(", "head_sha": ")" + sha + R"("}})";
}

static void test_artifacts()
{
    // The one holding playshelf.nro, by the name the workflow gives it, or
    // the name of the runs before; not a run's other artifacts.
    CHECK(dev_builds::is_build_artifact("playshelf-1.0.0-pr92-1f0a059-nro"));
    CHECK(dev_builds::is_build_artifact("playshelf-1.1.0-main-2bd4996-nro"));
    CHECK(dev_builds::is_build_artifact("playshelf_release"));
    CHECK(!dev_builds::is_build_artifact("playshelf-1.0.0-pr92-1f0a059-rescue"));
    CHECK(!dev_builds::is_build_artifact("playshelf-1.0.0-pr92-1f0a059-screenshots-default"));
    CHECK(!dev_builds::is_build_artifact("playshelf-1.0.0-pr92-1f0a059-nro.zip"));
    CHECK(!dev_builds::is_build_artifact("playshelf-nro") && !dev_builds::is_build_artifact("-nro") &&
          !dev_builds::is_build_artifact("") && !dev_builds::is_build_artifact("playshelf_packages"));

    const std::string a = "aaaaaaa" + std::string(33, '0'), b = "bbbbbbb" + std::string(33, '0'),
                      c = "ccccccc" + std::string(33, '0'), d = "ddddddd" + std::string(33, '0'),
                      e = "eeeeeee" + std::string(33, '0'), f = "fffffff" + std::string(33, '0');
    const std::string json = R"({"total_count": 11, "artifacts": [)" +
                             artifact("main", b, "2026-10-09T12:00:00Z") + "," +
                             artifact("main", a, "2026-10-08T12:00:00Z") + "," +
                             // Re-run (one per commit), under the name of the runs before.
                             artifact("main", a, "2026-10-08T12:05:00Z", 1, false, "playshelf_release") + "," +
                             artifact("feat/x", c, "2026-10-09T10:00:00Z") + "," +
                             artifact("feat/x", d, "2026-10-09T11:00:00Z") + "," +
                             artifact("main", e, "2026-10-09T13:00:00Z", 2) + "," +   // a fork's main: its PR
                             artifact("main", f, "2026-10-09T14:00:00Z", 1, true) + "," +   // expired
                             // The run's other artifacts.
                             artifact("main", f, "2026-10-09T14:00:00Z", 1, false, "playshelf-1.0.0-main-fffffff-rescue") + "," +
                             artifact("main", f, "2026-10-09T14:00:00Z", 1, false, "playshelf-1.0.0-main-fffffff-linker-map") + "," +
                             artifact("main", f, "2026-10-09T14:00:00Z", 1, false, "playshelf_linker_map") + "," +
                             R"({"name": "playshelf_release", "size_in_bytes": 1, "expired": false, "archive_download_url": "https://x", "workflow_run": {}})" +
                             "]}";
    std::vector<dev_builds::Artifact> arts;
    size_t listed = 0;
    CHECK(dev_builds::parse_artifacts(json, &arts, &listed));
    CHECK(arts.size() == 6 && listed == 11);
    CHECK(arts[0].commit == "bbbbbbb" && arts[0].branch == "main" && arts[0].repo == 1 && arts[0].head_repo == 1 &&
           arts[0].size == 4000 && arts[0].sha256 == std::string(64, 'd'));

    std::vector<dev_builds::Pull> pulls;
    CHECK(dev_builds::parse_pulls(R"([
        {"number": 41, "title": "feat: x", "head": {"ref": "feat/x", "sha": "x", "repo": {"id": 1}}},
        {"number": 42, "title": "fix: from a fork", "head": {"ref": "main", "repo": {"id": 2}}},
        {"number": 43, "title": "no build yet", "head": {"ref": "feat/y", "repo": {"id": 1}}},
        {"number": 44, "title": "deleted fork", "head": {"ref": "z", "repo": null}}
    ])", &pulls));
    CHECK(pulls.size() == 3 && pulls[0].number == 41 && pulls[1].head_repo == 2);

    const auto builds = dev_builds::combine(arts, pulls);
    // main newest first, one per commit; the fork's "main" is not main, and a
    // fork's pull request is not offered at all.
    CHECK(builds.size() == 3);
    CHECK(builds[0].kind == Kind::Main && builds[0].commit == "bbbbbbb" && builds[0].artifact);
    CHECK(builds[1].kind == Kind::Main && builds[1].commit == "aaaaaaa" && builds[1].date == "2026-10-08T12:05:00Z");
    // Pull requests by number, highest first, each its newest build.
    CHECK(builds[2].kind == Kind::PullRequest && builds[2].pr == 41 && builds[2].commit == "ddddddd");
    for (const auto& build : builds) CHECK(build.pr != 42);
    CHECK(dev_builds::combine(arts, pulls, 1).size() == 2);   // main kept to 1

    std::vector<dev_builds::Artifact> none;
    CHECK(!dev_builds::parse_artifacts("[]", &none) && !dev_builds::parse_artifacts("x", &none));
    std::vector<dev_builds::Pull> no_pulls;
    CHECK(!dev_builds::parse_pulls("{}", &no_pulls) && no_pulls.empty());
}

static void test_order()
{
    auto make = [](Kind kind, int pr, const char* commit, const char* date) {
        dev_builds::Build b;
        b.kind = kind;
        b.pr = pr;
        b.commit = commit;
        b.date = date;
        return b;
    };
    const std::vector<dev_builds::Build> all{
        make(Kind::Release, 0, "", "2026-10-01T09:00:00Z"),
        make(Kind::Main, 0, "aaaaaaa", "2026-10-08T12:00:00Z"),
        make(Kind::Main, 0, "bbbbbbb", "2026-10-09T12:00:00Z"),
        make(Kind::PullRequest, 41, "ccccccc", "2026-10-09T10:00:00Z"),
        make(Kind::PullRequest, 45, "ddddddd", "2026-10-07T10:00:00Z"),
        make(Kind::PullRequest, 43, "eeeeeee", "2026-10-09T11:00:00Z"),
        make(Kind::PullRequest, 44, "fffffff", "2026-10-09T11:00:00Z"),   // built with #43
    };
    using dev_builds::Order;
    auto commits = [](const std::vector<dev_builds::Build>& v) {
        std::string s;
        for (const auto& b : v) s += b.commit.empty() ? "r" : b.commit.substr(0, 1);
        return s;
    };
    // Each kind apart: the newest build first; a tie keeps the listed order.
    CHECK(commits(dev_builds::of_kind(all, Kind::Release, Order::Newest)) == "r");
    CHECK(commits(dev_builds::of_kind(all, Kind::Main, Order::Newest)) == "ba");
    CHECK(commits(dev_builds::of_kind(all, Kind::PullRequest, Order::Newest)) == "efcd");
    // By number: the pull requests only, highest first; main stays newest first.
    CHECK(commits(dev_builds::of_kind(all, Kind::PullRequest, Order::Number)) == "dfec");
    CHECK(commits(dev_builds::of_kind(all, Kind::Main, Order::Number)) == "ba");
    CHECK(dev_builds::of_kind({}, Kind::Main, Order::Newest).empty());

    CHECK(dev_builds::order_named("number") == Order::Number && dev_builds::order_named("newest") == Order::Newest);
    CHECK(dev_builds::order_named("") == Order::Newest && dev_builds::order_named("NUMBER") == Order::Newest);
    CHECK(std::string(dev_builds::order_name(Order::Number)) == "number" &&
          std::string(dev_builds::order_name(Order::Newest)) == "newest");
}

static void test_cache()
{
    Build rel;
    rel.kind = Kind::Release;
    rel.version = "1.2.0";
    rel.url = "https://github.com/o/r/releases/download/v1.2.0/playshelf.nro";
    rel.size = 5000;
    rel.sha256 = std::string(64, 'a');
    rel.prerelease = true;
    Build pr;
    pr.kind = Kind::PullRequest;
    pr.artifact = true;
    pr.pr = 48;
    pr.title = "fix: \"quoted\" title";
    pr.commit = "abc1234";
    pr.date = "2026-10-09T12:03:00Z";
    pr.url = "https://api.github.com/repos/o/r/actions/artifacts/1/zip";
    pr.size = 4000;
    pr.notes = "## What\nit does";
    pr.expires = "2027-01-07T12:03:00Z";
    pr.run = 77;
    pr.ci = dev_builds::Ci::Failed;
    pr.sha256 = std::string(64, 'b');
    Build bad = pr;
    bad.url = "http://x";   // not https: left out when read back
    // An edited list: anywhere but GitHub is left out too.
    // No digest, or a malformed one: it could not be installed, not kept.
    Build undigested = pr, upper = rel, short_digest = rel;
    undigested.sha256.clear();
    upper.sha256 = std::string(64, 'A');
    short_digest.sha256 = std::string(63, 'a');
    Build elsewhere = rel, lookalike = pr, userinfo = rel;
    elsewhere.url = "https://example.com/playshelf.nro";
    lookalike.url = "https://api.github.com.example.com/repos/o/r/actions/artifacts/1/zip";
    userinfo.url = "https://github.com@example.com/playshelf.nro";
    dev_builds::Cache c;
    c.builds = { rel, pr, bad, elsewhere, lookalike, userinfo, undigested, upper, short_digest };
    c.needs_login = false;
    c.fetched_at = 1000000;

    dev_builds::Cache back;
    CHECK(dev_builds::decode_cache(dev_builds::encode_cache(c), &back));
    CHECK(back.fetched_at == 1000000 && !back.needs_login && back.builds.size() == 2);
    CHECK(back.builds[0].kind == Kind::Release && back.builds[0].version == "1.2.0" && !back.builds[0].artifact &&
           back.builds[0].size == 5000 && back.builds[0].sha256 == rel.sha256 && back.builds[0].url == rel.url &&
           back.builds[0].prerelease);
    CHECK(back.builds[1].kind == Kind::PullRequest && back.builds[1].artifact && back.builds[1].pr == 48 &&
           back.builds[1].title == pr.title && back.builds[1].commit == "abc1234" && back.builds[1].date == pr.date &&
           back.builds[1].sha256 == pr.sha256 && !back.builds[1].prerelease && back.builds[1].notes == pr.notes &&
           back.builds[1].expires == pr.expires && back.builds[1].run == 77 && back.builds[1].ci == dev_builds::Ci::Failed);

    // Fresh: under ten minutes old, not from the future, same sign-in state, not empty.
    CHECK(dev_builds::cache_fresh(back, 1000000 + 599, false));
    CHECK(!dev_builds::cache_fresh(back, 1000000 + 600, false));
    CHECK(!dev_builds::cache_fresh(back, 1000000 - 1, false));
    CHECK(!dev_builds::cache_fresh(back, 1000000, true));
    back.builds.clear();
    CHECK(!dev_builds::cache_fresh(back, 1000000, false));

    dev_builds::Cache none;
    CHECK(!dev_builds::decode_cache("", &none) && !dev_builds::decode_cache("[]", &none) &&
           !dev_builds::decode_cache(R"({"version": 2, "builds": []})", &none));
    // Its schema, "version" in an earlier build's file: still read; another
    // (or none) refused, the list fetched again.
    CHECK(dev_builds::encode_cache(c).find("\"schema\": 1") != std::string::npos);
    CHECK(dev_builds::decode_cache(R"({"version": 1, "fetched_at": 5, "builds": []})", &none) && none.fetched_at == 5);
    CHECK(dev_builds::decode_cache(R"({"schema": 1, "fetched_at": 6, "mirror": "x", "builds": []})", &none) && none.fetched_at == 6);
    CHECK(!dev_builds::decode_cache(R"({"schema": 2, "builds": []})", &none) && !dev_builds::decode_cache(R"({"builds": []})", &none));
    CHECK(!dev_builds::cache_fresh(none, 0, false));
}

static std::string nro(size_t size)
{
    std::string s(size, 'z');
    s.replace(0x10, 4, "NRO0");
    return s;
}

static void test_files()
{
    const std::string dir = paths::data_dir();
    CHECK(paths::ensure_dir(dir));
    const std::string file = dir + "/fresh.nro";
    const std::string content = nro(5000);
    CHECK(paths::atomic_write(file, content));

    std::string err;
    CHECK(dev_builds::verify_file(file, content.size(), sha(content), &err));
    // No digest: refused, whatever the size and the header say.
    CHECK(!dev_builds::verify_file(file, content.size(), "", &err) && err == dev_builds::NO_DIGEST);
    CHECK(!dev_builds::verify_file(file, content.size(), sha(content + "x"), &err) && err == "SHA-256 does not match");
    CHECK(!dev_builds::verify_file(file, 4999, sha(content), &err) && err.find("size") == 0);
    CHECK(!dev_builds::verify_file(dir + "/missing.nro", 1, sha(content), &err));
    CHECK(dev_builds::verify_nro(file, &err));
    CHECK(paths::atomic_write(dir + "/zip.nro", std::string(5000, 'z')));
    CHECK(!dev_builds::verify_nro(dir + "/zip.nro", &err) && err == "not an NRO file");
    CHECK(!dev_builds::verify_nro(dir + "/missing.nro", &err));

    // Replace: the old one goes, the new one takes its name.
    const std::string target = dir + "/playshelf.nro";
    CHECK(paths::atomic_write(target, "old"));
    CHECK(dev_builds::replace(target, file, &err));
    std::string got;
    CHECK(paths::read_file(target, got) && got == content);
    CHECK(!paths::read_file(file, got) && !paths::read_file(target + ".old", got));
    // A missing new file: the old one stays in place.
    CHECK(!dev_builds::replace(target, dir + "/missing.nro", &err));
    CHECK(paths::read_file(target, got) && got == content);
    // No target yet: simply created.
    CHECK(paths::atomic_write(file, "new"));
    CHECK(dev_builds::replace(dir + "/first.nro", file, &err));
    CHECK(paths::read_file(dir + "/first.nro", got) && got == "new");
    // Kept as the previous build, the one kept before going.
    CHECK(paths::atomic_write(target + ".previous", "older"));
    CHECK(paths::atomic_write(file, "newest"));
    CHECK(dev_builds::replace(target, file, &err, target + ".previous"));
    CHECK(paths::read_file(target, got) && got == "newest");
    CHECK(paths::read_file(target + ".previous", got) && got == content);
    CHECK(!paths::read_file(target + ".old", got));
    // Nothing replaced (no target): no previous build made up, the one kept stays.
    CHECK(paths::atomic_write(file, "x"));
    CHECK(dev_builds::replace(dir + "/second.nro", file, &err, dir + "/second.nro.previous"));
    CHECK(!paths::read_file(dir + "/second.nro.previous", got));
}

static void test_zip()
{
    const std::string dir = paths::data_dir();
    const std::string nro_content = nro(300000);
    CHECK(paths::atomic_write(dir + "/playshelf.nro", nro_content));
    CHECK(paths::atomic_write(dir + "/build-info.txt", "commit: x\n"));
    // Built by Python's zipfile, deflated and stored, as upload-artifact would.
    const std::string py = "python3 -c \"import zipfile,sys; d=sys.argv[1]\n"
                           "for name, mode in (('a.zip', zipfile.ZIP_DEFLATED), ('s.zip', zipfile.ZIP_STORED)):\n"
                           "  z = zipfile.ZipFile(d + '/' + name, 'w', mode)\n"
                           "  z.write(d + '/build-info.txt', 'build-info.txt'); z.write(d + '/playshelf.nro', 'playshelf.nro'); z.close()\" ";
    CHECK(std::system((py + "'" + dir + "'").c_str()) == 0);

    std::string err, got;
    for (const char* name : { "/a.zip", "/s.zip" }) {
        const std::string out = dir + "/out.nro";
        CHECK(zip_read::extract(dir + name, "playshelf.nro", out, 1 << 20, &err));
        CHECK(paths::read_file(out, got) && got == nro_content);
        std::remove(out.c_str());
    }
    const std::string out = dir + "/out.nro";
    CHECK(!zip_read::extract(dir + "/a.zip", "missing.nro", out, 1 << 20, &err) && err.find("not in") != std::string::npos);
    CHECK(!zip_read::extract(dir + "/a.zip", "playshelf.nro", out, 1000, &err) && err.find("too large") != std::string::npos);
    CHECK(!zip_read::extract(dir + "/playshelf.nro", "playshelf.nro", out, 1 << 20, &err));
    CHECK(!zip_read::extract(dir + "/missing.zip", "playshelf.nro", out, 1 << 20, &err));

    // A changed byte inside the stored data: the CRC-32 catches it, nothing is left.
    std::string zip;
    CHECK(paths::read_file(dir + "/s.zip", zip));
    const size_t at = zip.find("NRO0");
    CHECK(at != std::string::npos);
    zip[at + 100] ^= 1;
    CHECK(paths::atomic_write(dir + "/bad.zip", zip));
    CHECK(!zip_read::extract(dir + "/bad.zip", "playshelf.nro", out, 1 << 20, &err) && err.find("CRC") != std::string::npos);
    CHECK(!paths::read_file(out, got));
    // Truncated: the directory is gone.
    CHECK(paths::atomic_write(dir + "/cut.zip", zip.substr(0, zip.size() / 2)));
    CHECK(!zip_read::extract(dir + "/cut.zip", "playshelf.nro", out, 1 << 20, &err));
}

static void test_auth()
{
    github_auth::DeviceCode d;
    CHECK(github_auth::parse_device_code(R"({"device_code": "3584d83", "user_code": "WDJB-MJHT",
        "verification_uri": "https://github.com/login/device", "expires_in": 900, "interval": 5})", &d));
    CHECK(d.device_code == "3584d83" && d.user_code == "WDJB-MJHT" && d.interval == 5 && d.expires_in == 900);
    CHECK(github_auth::parse_device_code(R"({"device_code": "x", "user_code": "Y", "verification_uri": "https://g",
        "expires_in": 999999, "interval": 0})", &d) && d.interval == 1 && d.expires_in == 3600);
    CHECK(!github_auth::parse_device_code(R"({"error": "unauthorized_client"})", &d));
    CHECK(!github_auth::parse_device_code(R"({"device_code": "x", "user_code": "Y", "verification_uri": "http://g"})", &d));
    CHECK(!github_auth::parse_device_code("nope", &d));

    std::string token, error;
    CHECK(github_auth::parse_poll(R"({"access_token": "ghu_16C7e42F292c6912E7710c838347Ae178B4a", "token_type": "bearer", "scope": ""})",
                                   &token, &error) == github_auth::Poll::Token);
    CHECK(token == "ghu_16C7e42F292c6912E7710c838347Ae178B4a");
    CHECK(github_auth::parse_poll(R"({"error": "authorization_pending"})", &token, &error) == github_auth::Poll::Pending);
    CHECK(github_auth::parse_poll(R"({"error": "slow_down", "interval": 10})", &token, &error) == github_auth::Poll::SlowDown);
    CHECK(github_auth::parse_poll(R"({"error": "expired_token"})", &token, &error) == github_auth::Poll::Expired);
    CHECK(github_auth::parse_poll(R"({"error": "access_denied"})", &token, &error) == github_auth::Poll::Denied);
    CHECK(github_auth::parse_poll(R"({"error": "incorrect_client_credentials"})", &token, &error) == github_auth::Poll::Failed &&
           error == "incorrect_client_credentials");
    // Nothing that could break an HTTP header is taken as a token.
    CHECK(github_auth::parse_poll("{\"access_token\": \"ghu_x\\r\\nX-Evil: 1\"}", &token, &error) == github_auth::Poll::Failed);
    CHECK(github_auth::parse_poll("garbage", &token, &error) == github_auth::Poll::Failed);

    // The token file: its own, not config.json; forgotten on sign-out.
    CHECK(github_auth::token().empty());
    CHECK(github_auth::token_file() == paths::data_dir() + "/github_token");
    CHECK(github_auth::save_token("ghu_abc123", &error));
    CHECK(github_auth::token() == "ghu_abc123");
    // The earlier OAuth app's token reads as none: signed in again.
    CHECK(github_auth::save_token("gho_old", &error) && github_auth::token().empty());
    CHECK(github_auth::save_token("ghu_abc123", &error));
    CHECK(!github_auth::save_token("gho abc", &error));
    CHECK(github_auth::token() == "ghu_abc123");
    // Sealed on the SD card, never as it is; an older version's plain file
    // reads, and is sealed where it lies.
    CHECK(fs::read(github_auth::token_file()).find("ghu_abc123") == std::string::npos);
    fs::write(github_auth::token_file(), "ghu_plain42\n");
    CHECK(github_auth::token() == "ghu_plain42");
    CHECK(fs::read(github_auth::token_file()).compare(0, 3, "v1:") == 0);
    CHECK(github_auth::token() == "ghu_plain42");
    // Another console's box (another key): no token.
    fs::write(github_auth::token_file(), "v1:EBESExQVFhcYGRobHB0eHzn0brSqJ5yDZT1qqcAHWbx15YzOV3BxNLyuym/x/ChxO5CWhbskRkKgfpLreaRBzXZtpw==\n");
    CHECK(github_auth::token().empty());
    CHECK(github_auth::save_token("ghu_abc123", &error));
    CHECK(paths::atomic_write(github_auth::token_file(), "not a token!\n"));
    CHECK(github_auth::token().empty());
    github_auth::forget_token();
    CHECK(github_auth::token().empty());
    // Refused by GitHub (401): forgotten, unless a new sign-in replaced it.
    CHECK(github_auth::save_token("ghu_new", &error));
    github_auth::forget_refused("ghu_abc123");
    CHECK(github_auth::token() == "ghu_new");
    github_auth::forget_refused("");
    CHECK(github_auth::token() == "ghu_new");
    github_auth::forget_refused("ghu_new");
    CHECK(github_auth::token().empty());
    // Only GitHub's 401 forgets it: 403 and the rest keep it.
    CHECK(github_auth::save_token("ghu_new", &error));
    CHECK(!github_auth::forget_if_revoked(403, "ghu_new") && !github_auth::forget_if_revoked(0, "ghu_new"));
    CHECK(github_auth::token() == "ghu_new");
    CHECK(github_auth::forget_if_revoked(401, "ghu_old") && github_auth::token() == "ghu_new");   // a sign-in since
    CHECK(github_auth::forget_if_revoked(401, "ghu_new") && github_auth::token().empty());
    // White space after it (an edited file) is not part of it; before it is.
    CHECK(paths::atomic_write(github_auth::token_file(), "ghu_x \t\r\n"));
    CHECK(github_auth::token() == "ghu_x");
    CHECK(paths::atomic_write(github_auth::token_file(), " ghu_x\n"));
    CHECK(github_auth::token().empty());
    github_auth::forget_token();

    const auto h = github_auth::api_headers("ghu_abc");
    CHECK(h.size() == 3 && h[2] == "Authorization: Bearer ghu_abc");
    CHECK(github_auth::api_headers("").size() == 2);
    CHECK(github_auth::api_url("gists") == "https://api.github.com/gists");
    CHECK(github_auth::api_url("repos/o/r/releases") == "https://api.github.com/repos/o/r/releases");
}

int main()
{
    TempDir tmp("dev_builds", TempDir::Enter);   // paths::data_dir() is ./playshelf_data on the host

    test_sha256();
    test_hmac_sha256();
    test_names();
    test_release();
    test_releases();
    test_find_and_runs();
    test_artifacts();
    test_order();
    test_cache();
    test_files();
    test_zip();
    test_auth();

    tmp.remove();
    return CHECK_DONE("sha256, HMAC-SHA256, release and artifact lists, the order of each kind, the cached list, download checks, zip extraction, replacement and GitHub sign-in assertions passed");
}
