// Host tests for source/util/http_logic.cpp: the size limit of a response, the
// headers a request sends, the error a failed transfer reports, what a
// download does with the answer's status and what counts as a redirect.
#include "check.h"
#include <string>
#include <vector>

#include "util/http_logic.hpp"

static void test_fits()
{
    CHECK(http_logic::fits(0, 0, 0));
    CHECK(http_logic::fits(0, 100, 100));     // exactly the limit is fine
    CHECK(!http_logic::fits(0, 101, 100));
    CHECK(http_logic::fits(60, 40, 100));
    CHECK(!http_logic::fits(60, 41, 100));    // the chunk that crosses it stops the transfer
    CHECK(!http_logic::fits(100, 1, 100));
}

static void test_headers()
{
    CHECK(http_logic::request_headers(nullptr, nullptr).empty());   // a plain GET
    const std::vector<std::string> extra = { "Authorization: Bearer abc", "Accept: application/json" };
    CHECK(http_logic::request_headers(nullptr, &extra) == extra);
    const auto post = http_logic::request_headers("application/x-www-form-urlencoded", &extra);
    REQUIRE(post.size() == 4);
    CHECK(post[0] == "Content-Type: application/x-www-form-urlencoded");
    CHECK(post[1] == "Expect:");               // no 100-continue round trip
    CHECK(post[2] == extra[0] && post[3] == extra[1]);
    const std::vector<std::string> none;
    CHECK(http_logic::request_headers("text/plain", &none).size() == 2);
}

static void test_outcome()
{
    http_logic::Outcome o;
    o.status = 200;
    std::string error = "untouched";
    CHECK(http_logic::succeeded(o, &error) && error == "untouched");
    CHECK(http_logic::succeeded(o, nullptr));
    o.status = 0;                              // no status read, transfer complete: only the transfer decides
    CHECK(http_logic::succeeded(o, &error));
    o.status = 302;                            // a redirect not followed (redirect()): not an error here
    CHECK(http_logic::succeeded(o, &error));

    // The status wins over everything else: a refusal's large HTML page
    // overflows, but the user should read "HTTP 403".
    o.status = 403;
    o.overflow = true;
    o.transfer_ok = false;
    o.transfer_error = "Failure writing output to destination";
    CHECK(!http_logic::succeeded(o, &error) && error == "HTTP 403");
    CHECK(!http_logic::succeeded(o, nullptr));

    o.status = 200;
    CHECK(!http_logic::succeeded(o, &error) && error == "response too large");
    o.overflow = false;
    o.file_error = true;
    CHECK(!http_logic::succeeded(o, &error) && error == "cannot write the file");
    o.file_error = false;
    CHECK(!http_logic::succeeded(o, &error) && error == "Failure writing output to destination");

    http_logic::Outcome timeout;
    timeout.transfer_ok = false;
    timeout.transfer_error = "Operation timed out after 10001 milliseconds";
    CHECK(!http_logic::succeeded(timeout, &error) && error == timeout.transfer_error);
    http_logic::Outcome server;
    server.status = 503;
    CHECK(!http_logic::succeeded(server, &error) && error == "HTTP 503");

    // A Range request answered in full: libcurl stops it (CURLE_RANGE_ERROR)
    // with a message that changed between versions, so the error is one of
    // PlayShelf's own, and a resumed download starts over on it as on a 416.
    http_logic::Outcome range;
    range.status = 200;
    range.transfer_ok = false;
    range.range_error = true;
    range.transfer_error = "HTTP server doesn't seem to support byte ranges. Cannot resume.";
    CHECK(!http_logic::succeeded(range, &error) && error == http_logic::RANGE_REFUSED);
    CHECK(http_logic::restart_resume(error));
    range.transfer_error = "HTTP server does not seem to support byte ranges. Cannot resume.";
    CHECK(!http_logic::succeeded(range, &error) && error == http_logic::RANGE_REFUSED);
    CHECK(http_logic::restart_resume("HTTP 416"));
    CHECK(!http_logic::restart_resume("HTTP 404"));
    CHECK(!http_logic::restart_resume(timeout.transfer_error));
    CHECK(!http_logic::restart_resume("cancelled"));
}

static void test_download_start()
{
    using http_logic::Start;
    // A new download: written as it comes, unless it is an error page.
    CHECK(http_logic::download_start(200, false) == Start::Write);
    CHECK(http_logic::download_start(0, false) == Start::Write);
    CHECK(http_logic::download_start(404, false) == Start::Refuse);
    CHECK(http_logic::download_start(503, false) == Start::Refuse);
    // A resumed one: the rest appended (206), all of it again (200) from an
    // empty file; an error, or any other answer, leaves the part as it was.
    CHECK(http_logic::download_start(206, true) == Start::Write);
    CHECK(http_logic::download_start(200, true) == Start::StartOver);
    CHECK(http_logic::download_start(416, true) == Start::Refuse);
    CHECK(http_logic::download_start(500, true) == Start::Refuse);
    CHECK(http_logic::download_start(503, true) == Start::Refuse);
    CHECK(http_logic::download_start(403, true) == Start::Refuse);
    CHECK(http_logic::download_start(204, true) == Start::Refuse);
    CHECK(http_logic::download_start(0, true) == Start::Refuse);

    // A refused answer below 400 says what came.
    http_logic::Outcome o;
    o.status = 204;
    o.refused = true;
    o.transfer_ok = false;
    o.transfer_error = "Failure writing output to destination";
    std::string error;
    CHECK(!http_logic::succeeded(o, &error) && error == "unexpected answer (HTTP 204)");
    o.status = 503;   // an error page: the status, as ever
    CHECK(!http_logic::succeeded(o, &error) && error == "HTTP 503");
}

static void test_redirect()
{
    std::string error;
    CHECK(http_logic::redirect_ok(302, "https://objects.githubusercontent.com/x.zip", &error));
    CHECK(http_logic::redirect_ok(301, "https://a", nullptr));
    CHECK(http_logic::redirect_ok(399, "https://a", nullptr));
    CHECK(!http_logic::redirect_ok(200, "", &error) && error == "no redirect (HTTP 200)");
    CHECK(!http_logic::redirect_ok(400, "https://a", &error) && error == "no redirect (HTTP 400)");
    CHECK(!http_logic::redirect_ok(299, "https://a", nullptr));
    // Never anywhere but https: no downgrade, no other scheme.
    CHECK(!http_logic::redirect_ok(302, "http://objects.githubusercontent.com/x.zip", &error) &&
          error == "no redirect (HTTP 302)");
    CHECK(!http_logic::redirect_ok(302, "HTTPS://upper.example", nullptr));
    CHECK(!http_logic::redirect_ok(302, "/relative/path", nullptr));
    CHECK(!http_logic::redirect_ok(302, "https:/", nullptr));
    CHECK(!http_logic::redirect_ok(302, "", nullptr));
}

static void test_content_range()
{
    http_logic::ContentRange r;
    CHECK(http_logic::parse_content_range("Content-Range: bytes 0-0/1234\r\n", &r));
    CHECK(r.first == 0 && r.last == 0 && r.total == 1234);
    CHECK(http_logic::parse_content_range("content-range:bytes 100-199/40000000000", &r));
    CHECK(r.first == 100 && r.last == 199 && r.total == 40000000000ull);
    CHECK(http_logic::parse_content_range("CONTENT-RANGE:   bytes 5-5/6  ", &r) && r.last == 5);
    // Not that header, a size unknown, a range out of the total, junk.
    CHECK(!http_logic::parse_content_range("Content-Length: 1234", &r));
    CHECK(!http_logic::parse_content_range("Content-Range: bytes 0-0/*", &r));
    CHECK(!http_logic::parse_content_range("Content-Range: bytes 0-1234/1234", &r));
    CHECK(!http_logic::parse_content_range("Content-Range: bytes 9-3/1234", &r));
    CHECK(!http_logic::parse_content_range("Content-Range: bytes -3/1234", &r));
    CHECK(!http_logic::parse_content_range("Content-Range: items 0-1/2", &r));
    CHECK(!http_logic::parse_content_range("Content-Range: bytes 0-1/2 extra", &r));
    CHECK(!http_logic::parse_content_range("Content-Range: bytes 0-1/99999999999999999999999", &r));

    std::string error;
    http_logic::ContentRange got{100, 199, 1000};
    CHECK(http_logic::range_ok(206, &got, 100, 100, &error));
    CHECK(!http_logic::range_ok(200, &got, 100, 100, &error) && error.find("HTTP 200") != std::string::npos);
    CHECK(!http_logic::range_ok(206, nullptr, 100, 100, &error));
    CHECK(!http_logic::range_ok(206, &got, 0, 100, &error));
    CHECK(!http_logic::range_ok(206, &got, 100, 99, &error));
}

int main()
{
    test_fits();
    test_headers();
    test_outcome();
    test_download_start();
    test_redirect();
    test_content_range();
    return CHECK_DONE("http_logic size limit, headers, outcome, download start, redirect and content range tests passed");
}
