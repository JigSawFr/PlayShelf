// library/services.cpp against a scripted AeroFoil: the web sign-in (its
// cookie, a refusal, a session over and signed in again once); the cheats'
// list, a cheat's file (the credentials to its own server only), where it
// goes on the SD card; the save backups listed, sent (a form with the
// archive), fetched to a file and deleted; the requests listed and made.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <map>

#include "check.h"
#include "fs.hpp"
#include "library/services.hpp"
#include "temp_dir.hpp"

using namespace library;

namespace
{
const std::string BASE = "https://shelf.example.org";

// Answers by "METHOD url" (GET for get()); missing: 404. The cookie the
// signed-in pages want, else a redirect to the sign-in.
struct Aero : Transport
{
    std::map<std::string, Answer> answers;
    std::string                   password = "secret";
    std::string                   live_cookie = "session=abc";
    int                           sign_ins = 0;
    std::vector<std::string>      asked;
    std::vector<std::string>      last_headers;
    http::Call                    last_call;

    Answer get(const std::string& url, const std::vector<std::string>& headers, const std::string& pin) override
    {
        http::Call c;
        return call(url, c, headers, pin);
    }
    Answer call(const std::string& url, const http::Call& c, const std::vector<std::string>& headers,
                const std::string&) override
    {
        const std::string method = c.method.empty() ? "GET" : c.method;
        asked.push_back(method + " " + url);
        last_headers = headers;
        last_call = c;
        Answer a;
        if (method == "POST" && url == BASE + "/login") {
            sign_ins++;
            a.status = 302;
            if (c.body == "user=kid&password=" + password) {
                a.location = "/";
                a.cookies = {live_cookie, "remember_token=1"};
            } else {
                a.location = "/login";
            }
            return a;
        }
        const bool session_page = url.rfind(BASE + "/api/cheats", 0) == 0 && url.size() == (BASE + "/api/cheats").size();
        if (session_page || url == BASE + "/api/requests") {
            bool signed_in = false;
            for (const std::string& h : headers)
                if (h.rfind("Cookie: " + live_cookie, 0) == 0) signed_in = true;
            if (!signed_in) {
                a.status = 302;
                a.location = "/login?next=%2Fapi%2Fcheats";
                return a;
            }
        }
        if (!c.to_path.empty()) {
            const auto it = answers.find(method + " " + url);
            if (it == answers.end()) {
                a.status = 404;
                return a;
            }
            fs::write(c.to_path, it->second.body);
            a.status = it->second.status;
            return a;
        }
        const auto it = answers.find(method + " " + url);
        if (it != answers.end()) return it->second;
        a.status = 404;
        return a;
    }
    bool        reachable(const std::string&) override { return true; }
    std::string certificate(const std::string&, std::string* why) override
    {
        *why = "none";
        return "";
    }
};

Answer ok(const std::string& body, long status = 200)
{
    Answer a;
    a.status = status;
    a.body = body;
    return a;
}

Link link()
{
    Server s;
    s.username = "kid";
    Fetch f;
    f.endpoint = BASE + "/";
    return link_of(s, f, "secret");
}

void test_sign_in()
{
    Aero    t;
    Session s;
    std::string why;
    CHECK(sign_in(link(), t, &s, &why) && s.cookie == "session=abc; remember_token=1");
    CHECK(t.last_call.content_type == "application/x-www-form-urlencoded" && !t.last_call.follow);
    t.password = "other";
    Session refused;
    CHECK(!sign_in(link(), t, &refused, &why) && why.find("username or password") != std::string::npos);
    Link anon = link();
    anon.username.clear();
    CHECK(!sign_in(anon, t, &refused, &why) && why.find("username") != std::string::npos);
    // A username and password with characters a form escapes.
    Link odd = link();
    odd.username = "a b";
    odd.password = "p&q=";
    (void)sign_in(odd, t, &refused, &why);
    CHECK(t.last_call.body == "user=a%20b&password=p%26q%3D");
}

void test_cheats()
{
    TempDir tmp("library_services", TempDir::Enter);
    Aero    t;
    t.answers["GET " + BASE + "/api/cheats"] = ok(R"({"success":true,"cheats":[
        {"name":"Game — 0123ABCD","title_name":"Game","title_id":"0100000000010000","build_id":"0123abcd",
         "url":"/api/cheats/0100000000010000/0123ABCD","size":42,"note":"60 fps"},
        {"title_id":"zz","build_id":"01","url":"/x"},
        {"title_id":"0100000000020000","build_id":"not hex","url":"/y"}]})");
    t.answers["GET " + BASE + "/api/cheats/0100000000010000/0123ABCD"] = ok("[Infinite HP]\n04000000 00 00\n");
    Session                s;
    std::vector<Cheat>     list;
    std::string            why;
    if (!CHECK(list_cheats(link(), t, s, &list, &why) && list.size() == 1)) {
        fprintf(stderr, "  %s\n", why.c_str());
        return;
    }
    // Signed in first (the list is a signed-in page), then the list.
    CHECK(t.sign_ins == 1 && list[0].build_id == "0123ABCD" && list[0].note == "60 fps" && list[0].size == 42);
    CHECK(list[0].url == BASE + "/api/cheats/0100000000010000/0123ABCD");
    // The session holds: not signed in again.
    CHECK(list_cheats(link(), t, s, &list, &why) && t.sign_ins == 1);
    // The session over: signed in again, once.
    t.live_cookie = "session=new";
    CHECK(list_cheats(link(), t, s, &list, &why) && t.sign_ins == 2 && s.cookie.rfind("session=new", 0) == 0);
    std::string text;
    CHECK(fetch_cheat(link(), t, list[0], &text, &why) && text.find("Infinite HP") != std::string::npos);
    CHECK(!t.last_headers.empty());   // the credentials, to their own server
    Cheat elsewhere = list[0];
    elsewhere.url = "https://other.example/c.txt";
    t.answers["GET https://other.example/c.txt"] = ok("x");
    CHECK(fetch_cheat(link(), t, elsewhere, &text, &why) && t.last_headers.empty());
    // A refusal answered as JSON.
    t.answers["GET " + BASE + "/api/cheats/0100000000010000/0123ABCD"] = ok(R"({"error":"Cheat access is disabled for this account."})");
    CHECK(!fetch_cheat(link(), t, list[0], &text, &why) && why.find("disabled") != std::string::npos);
    // No permission for the list.
    t.answers["GET " + BASE + "/api/cheats"] = ok(R"({"success":false,"error":"Cheat access is disabled for this account."})", 403);
    CHECK(!list_cheats(link(), t, s, &list, &why) && why.find("disabled") != std::string::npos);
    // On the SD card.
    CHECK(cheat_path("/sd/", 0x0100000000010000ull, "0123abcd") == "/sd/atmosphere/contents/0100000000010000/cheats/0123ABCD.txt");
    CHECK(cheat_path("/sd", 0x0100000000010000ull, "../x").empty() && cheat_path("/sd", 0, "01").empty());
    CHECK(normal_build_id(std::string(65, 'a')).empty());
    const std::string root = tmp.path();
    CHECK(installed_cheats(root, 0x0100000000010000ull).empty());
    fs::make_dirs(root + "/atmosphere/contents/0100000000010000/cheats");
    fs::write(root + "/atmosphere/contents/0100000000010000/cheats/0123ABCD.txt", "x");
    fs::write(root + "/atmosphere/contents/0100000000010000/cheats/notes.txt.bak", "x");
    fs::write(root + "/atmosphere/contents/0100000000010000/cheats/ffff0000.txt", "x");
    const auto have = installed_cheats(root, 0x0100000000010000ull);
    CHECK(have.size() == 2 && have[0] == "0123ABCD" && have[1] == "FFFF0000");
}

void test_saves()
{
    TempDir tmp("library_services_saves", TempDir::Enter);
    Aero    t;
    t.answers["GET " + BASE + "/api/saves/list"] = ok(R"({"saves":[
        {"title_id":"0100000000010000","title_name":"Game","save_id":"20260101T000000Z_aa","note":"before boss",
         "created_ts":100,"size":10,"is_latest":false,"download_url":"/api/saves/download/0100000000010000/20260101T000000Z_aa.zip",
         "delete_url":"/api/saves/delete/0100000000010000/20260101T000000Z_aa"},
        {"title_id":"0100000000010000","save_id":"latest","created_ts":300,"latest_directory":true,
         "download_url":"/api/saves/download/0100000000010000/latest.zip","delete_url":""},
        {"title_id":"0100000000010000","save_id":"20260102T000000Z_bb","created_ts":200,"is_latest":true,
         "download_url":"/api/saves/download/0100000000010000/20260102T000000Z_bb.zip",
         "delete_url":"/api/saves/delete/0100000000010000/20260102T000000Z_bb"},
        {"title_id":"0100000000010000","download_url":"http://plain/x.zip"}]})");
    std::vector<SaveBackup> list;
    std::string             why;
    if (!CHECK(list_saves(link(), t, &list, &why) && list.size() == 3)) {
        fprintf(stderr, "  %s\n", why.c_str());
        return;
    }
    // Newest first, the server's own copy last; credentials, no sign-in.
    CHECK(list[0].save_id == "20260102T000000Z_bb" && list[0].latest && list[1].note == "before boss");
    CHECK(list[2].server_copy && list[2].delete_url.empty() && list[1].title_name == "Game");
    CHECK(t.sign_ins == 0 && !t.last_headers.empty());
    // Sent: a form, the archive its file part, the note bounded.
    t.answers["POST " + BASE + "/api/saves/upload/0100000000010000"] = ok(R"({"success":true})");
    const std::string zip = tmp.path() + "/s.zip";
    fs::write(zip, "PK");
    CHECK(upload_save(link(), t, 0x0100000000010000ull, zip, "line\none " + std::string(300, 'x'), nullptr, nullptr, &why));
    CHECK(t.last_call.file_path == zip && t.last_call.file_field == "file" && t.last_call.file_name == "0100000000010000.zip");
    bool noted = false;
    for (const auto& [k, v] : t.last_call.fields)
        if (k == "note") noted = v.size() <= MAX_NOTE && v.find('\n') == std::string::npos;
    CHECK(noted);
    t.answers["POST " + BASE + "/api/saves/upload/0100000000010000"] = ok(R"({"success":false,"message":"Backup access is required for save sync."})", 403);
    CHECK(!upload_save(link(), t, 0x0100000000010000ull, zip, "", nullptr, nullptr, &why) && why.find("Backup access") != std::string::npos);
    // Fetched to a file.
    t.answers["GET " + list[0].download_url] = ok("PKzip");
    const std::string got = tmp.path() + "/got.zip";
    CHECK(download_save(link(), t, list[0], got, nullptr, nullptr, &why) && fs::read(got) == "PKzip");
    // Deleted; the server's own copy cannot be.
    t.answers["DELETE " + list[0].delete_url] = ok(R"({"success":true})");
    CHECK(delete_save(link(), t, list[0], &why));
    CHECK(!delete_save(link(), t, list[2], &why));
    // No Backup permission.
    t.answers["GET " + BASE + "/api/saves/list"] = ok(R"({"success":false,"error":"Backup access is required for save sync."})", 403);
    CHECK(!list_saves(link(), t, &list, &why) && why.find("Backup permission") != std::string::npos);
}

void test_requests()
{
    Aero t;
    t.answers["GET " + BASE + "/api/requests"] = ok(R"({"success":true,"requests":[
        {"id":1,"created_at":10,"status":"closed","title_id":"0100000000010000","title_name":"Old"},
        {"id":2,"created_at":20,"status":"open","title_id":"0100000000020000","title_name":null},
        {"id":3,"title_id":"bad"}]})");
    t.answers["POST " + BASE + "/api/requests"] = ok(R"({"success":true,"message":"Request created.","request_id":4})");
    Session                   s;
    std::vector<TitleRequest> list;
    std::string               why, message;
    CHECK(list_requests(link(), t, s, &list, &why) && list.size() == 2 && list[0].id == 2 && list[0].status == "open");
    CHECK(request_title(link(), t, s, 0x0100000000030000ull, "New \"one\"", &message, &why) && message == "Request created.");
    CHECK(t.last_call.content_type == "application/json" &&
          t.last_call.body == R"({"title_id":"0100000000030000","title_name":"New \"one\""})");
    t.answers["POST " + BASE + "/api/requests"] = ok(R"({"success":false,"message":"Title is already in the library."})");
    CHECK(!request_title(link(), t, s, 0x0100000000030000ull, "", &message, &why) && why == "Title is already in the library.");
    CHECK(t.sign_ins == 1);
}
}   // namespace

int main()
{
    test_sign_in();
    test_cheats();
    test_saves();
    test_requests();
    return CHECK_DONE("library services: sign-in, cheats, save backups and requests passed");
}
