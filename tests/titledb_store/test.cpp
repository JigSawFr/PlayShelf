// titledb/store.cpp: a source brought to its manifest's version over a
// scripted network (its packs fetched, a cut part resumed, a pack whose
// SHA-256 differs refused and the one in use kept), the icons pack the same
// from one version to the next not fetched again, the old packs removed;
// a title list made a pack, written once per version of it; sources by
// priority, the first that names a title winning.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "temp_dir.hpp"
#include "titledb/packs.hpp"
#include "titledb/store.hpp"
#include "util/paths.hpp"
#include "util/sha256.hpp"

#include <map>

using namespace titledb;
using namespace packs;

namespace
{
std::string hex(const std::string& bytes)
{
    Sha256 h;
    h.update(bytes.data(), bytes.size());
    return h.hex();
}

// A server: its files by address, what was asked of it.
struct Server
{
    std::map<std::string, std::string> files;
    std::vector<std::string>           asked;
    size_t                             cut_after = 0;   // a transfer cut after so many bytes (0: never)
    uint64_t                           free = 1ull << 40;

    Net net()
    {
        Net n;
        n.save = [this](const std::string& url, const std::string& path, uint64_t max, const std::function<void(uint64_t, uint64_t)>&,
                        std::string* why) {
            const auto it = this->files.find(url);
            if (it == this->files.end()) return (*why = "HTTP 404", false);
            if (it->second.size() > max) return (*why = "too large", false);
            std::ofstream(path, std::ios::binary) << it->second;
            return true;
        };
        n.file = [this](const std::string& url, const std::string& path, uint64_t size, uint64_t from,
                        const std::function<void(uint64_t)>& progress, std::string* why) {
            this->asked.push_back(url + "@" + std::to_string(from));
            const auto it = this->files.find(url);
            if (it == this->files.end()) return (*why = "HTTP 404", false);
            std::string rest = it->second.substr((size_t)std::min<uint64_t>(from, it->second.size()));
            const bool  cut = this->cut_after && rest.size() > this->cut_after;
            if (cut) rest.resize(this->cut_after);
            std::ofstream(path, from ? std::ios::binary | std::ios::app : std::ios::binary) << rest;
            progress(rest.size());
            (void)size;
            if (cut) return (*why = "the connection was reset", false);
            return true;
        };
        n.free_space = [this]() { return this->free; };
        return n;
    }
};

std::string manifest(const std::string& version, const std::string& titles, const std::string& icons)
{
    std::string m = R"({"db_version":")" + version + R"(","files":{"titles.pack":{"url":"titles.pack","size":)" +
                    std::to_string(titles.size()) + R"(,"sha256":")" + hex(titles) + R"("})";
    if (!icons.empty())
        m += R"(,"icons.pack":{"url":"https://cdn.test/icons.pack","size":)" + std::to_string(icons.size()) + R"(,"sha256":")" +
             hex(icons) + R"("})";
    return m + "}}";
}

const char* const URL = "https://db.test/m.json";
const uint64_t    ZELDA = 0x01007EF00011E000, MARIO = 0x0100000000010000;

void test_update()
{
    TempDir           tmp("titledb_store", TempDir::Enter);
    const std::string dir = source_dir(tmp.path(), URL);
    Server            s;
    const std::string t1 = titles_pack({ { MARIO, "Mario", "Nintendo", 0 } });
    const std::string icons = icons_pack({ { MARIO, std::string(5000, 'i') } });
    s.files[URL] = manifest("1", t1, icons);
    s.files["https://db.test/titles.pack"] = t1;
    s.files["https://cdn.test/icons.pack"] = icons;

    std::string why;
    uint64_t    last = 0, total = 0;
    CHECK(update(URL, dir, s.net(), [&](uint64_t d, uint64_t t) { last = d, total = t; }, &why) == Outcome::Updated);
    CHECK(total == t1.size() + icons.size() && last == total);
    State st;
    CHECK(read_state(dir, &st) && st.version == "1" && st.count == 1 && st.url == URL);
    CHECK(paths::file_exists(dir + "/" + st.titles) && paths::file_exists(dir + "/" + st.icons));
    // The same version: nothing fetched.
    s.asked.clear();
    CHECK(update(URL, dir, s.net(), nullptr, &why) == Outcome::Current && s.asked.empty());

    // A new version, its icons the same: only the titles pack fetched; the
    // old one gone.
    const std::string t2 = titles_pack({ { MARIO, "Mario", "Nintendo", 0 }, { ZELDA, "Zelda", "Nintendo", 0 } });
    s.files[URL] = manifest("2", t2, icons);
    s.files["https://db.test/titles.pack"] = t2;
    const std::string old = st.titles;
    CHECK(update(URL, dir, s.net(), nullptr, &why) == Outcome::Updated);
    CHECK(s.asked == std::vector<std::string>{ "https://db.test/titles.pack@0" });
    CHECK(read_state(dir, &st) && st.version == "2" && st.count == 2 && !paths::file_exists(dir + "/" + old));

    // Cut: the part kept, resumed from where it stopped.
    const std::string t3 = titles_pack({ { MARIO, "Mario 3", "", 0 } });
    s.files[URL] = manifest("3", t3, icons);
    s.files["https://db.test/titles.pack"] = t3;
    s.cut_after = 40;
    s.asked.clear();
    CHECK(update(URL, dir, s.net(), nullptr, &why) == Outcome::Failed && why.find("reset") != std::string::npos);
    s.cut_after = 0;
    CHECK(update(URL, dir, s.net(), nullptr, &why) == Outcome::Updated);
    CHECK(s.asked.size() == 2 && s.asked[1] == "https://db.test/titles.pack@40");
    CHECK(read_state(dir, &st) && st.version == "3");

    // A pack that is not what the manifest says: refused, the one in use kept.
    s.files[URL] = manifest("4", titles_pack({ { MARIO, "Mario 4", "", 0 } }), icons);
    s.files["https://db.test/titles.pack"] = titles_pack({ { MARIO, "Evil", "", 0 } });
    CHECK(update(URL, dir, s.net(), nullptr, &why) == Outcome::Failed && why.find("SHA-256") != std::string::npos);
    CHECK(read_state(dir, &st) && st.version == "3" && paths::file_exists(dir + "/" + st.titles));
    CHECK(paths::list_files(dir, ".part").empty());

    // No room, no manifest, a manifest over HTTP.
    s.files["https://db.test/titles.pack"] = titles_pack({ { MARIO, "Mario 4", "", 0 } });
    s.free = 10;
    CHECK(update(URL, dir, s.net(), nullptr, &why) == Outcome::Failed && why == "not enough free space");
    s.free = 1ull << 40;
    CHECK(update("https://db.test/none.json", dir, s.net(), nullptr, &why) == Outcome::Failed);
    s.files[URL] = R"({"version":"5","titles":{"url":"http://db.test/t.pack","size":3,"sha256":")" + std::string(64, 'a') + "\"}}";
    CHECK(update(URL, dir, s.net(), nullptr, &why) == Outcome::Failed && why.find("HTTPS") != std::string::npos);

    // Removed with its source.
    CHECK(remove_source(dir) && !paths::file_exists(dir + "/state.json"));
}

void test_list_source()
{
    TempDir           tmp("titledb_list_source", TempDir::Enter);
    const char* const LIST = "https://list.test/owned.json";
    const std::string dir = source_dir(tmp.path(), LIST);
    Server            s;
    s.files[LIST] = R"({"0100000000010000": {"Game Name": "Mario", "Version": "0", "Size": 5}})";
    std::string why;
    CHECK(update(LIST, dir, s.net(), nullptr, &why) == Outcome::Updated);
    State st;
    CHECK(read_state(dir, &st) && st.list && st.count == 1 && st.icons.empty() && st.version.size() == 16);
    // Nothing fetched as a pack; the same list again: nothing written.
    CHECK(s.asked.empty() && update(LIST, dir, s.net(), nullptr, &why) == Outcome::Current);
    // A new list: its pack, the old one gone.
    const std::string old = st.titles;
    s.files[LIST] = R"({"0100000000010000": {"Game Name": "Mario 2"}})";
    CHECK(update(LIST, dir, s.net(), nullptr, &why) == Outcome::Updated);
    CHECK(read_state(dir, &st) && st.titles != old && !paths::file_exists(dir + "/" + old));
    Database db;
    db.open({ dir });
    CHECK(db.title(MARIO) && db.title(MARIO)->name == "Mario 2");
    // Neither a manifest nor a list.
    s.files[LIST] = R"({"hello": "world"})";
    CHECK(update(LIST, dir, s.net(), nullptr, &why) == Outcome::Failed && !why.empty());
}

void test_priority()
{
    TempDir           tmp("titledb_priority", TempDir::Enter);
    Server            s;
    const std::string mine = titles_pack({ { MARIO, "My Mario", "", 0 }, { ZELDA, "", "Someone", 0 } });
    const std::string theirs = titles_pack({ { MARIO, "Mario", "Nintendo", 0 }, { ZELDA, "Zelda", "Nintendo", 0 } });
    const std::string icons = icons_pack({ { ZELDA, "zelda-icon" } });
    s.files["https://mine.test/m.json"] = manifest("1", mine, "");
    s.files["https://mine.test/titles.pack"] = mine;
    s.files[URL] = manifest("1", theirs, icons);
    s.files["https://db.test/titles.pack"] = theirs;
    s.files["https://cdn.test/icons.pack"] = icons;
    const std::string a = source_dir(tmp.path(), "https://mine.test/m.json"), b = source_dir(tmp.path(), URL);
    std::string       why;
    CHECK(update("https://mine.test/m.json", a, s.net(), nullptr, &why) == Outcome::Updated);
    CHECK(update(URL, b, s.net(), nullptr, &why) == Outcome::Updated);

    Database db;
    db.open({ a, b, tmp.path() + "/missing" });
    CHECK(db.sources().size() == 2);
    // The first source names Mario; it has no name for Zelda: the second's.
    const auto m = db.title(MARIO), z = db.title(ZELDA);
    CHECK(m && m->name == "My Mario" && z && z->name == "Zelda" && z->publisher == "Nintendo");
    CHECK(!db.title(0x0100000000099000));
    std::vector<uint8_t> bytes;
    std::string          ext;
    CHECK(db.icon(ZELDA, &bytes, &ext) && std::string(bytes.begin(), bytes.end()) == "zelda-icon" && !db.icon(MARIO, &bytes, &ext));
    // The other order: theirs first.
    db.open({ b, a });
    CHECK(db.title(MARIO)->name == "Mario");
}
}   // namespace

int main()
{
    test_update();
    test_list_source();
    test_priority();
    return CHECK_DONE("title database store: updates, resume, a hash refused, no room, title lists, priority passed");
}
