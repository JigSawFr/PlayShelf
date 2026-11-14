// saves/local_backups.cpp: the names of a game's local save copies, and which to delete.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "saves/local_backups.hpp"

using namespace local_backups;

int main()
{
    CHECK(dir_of("/sd/config/playshelf", 0x0100000000010000ull) == "/sd/config/playshelf/saves/0100000000010000");
    CHECK(user_tag(1, 2).size() == 16 && user_tag(1, 2) == user_tag(1, 2) && user_tag(1, 2) != user_tag(2, 1));

    const std::string tag = user_tag(0x1111, 0x2222), other = user_tag(0x3333, 0x4444);
    const std::string n1 = file_name("2026-11-14", "11:12:54", tag);
    CHECK(n1 == "2026-11-14_11-12-54_" + tag + ".zip");
    std::string got;
    CHECK(is_ours(n1, &got) && got == tag);

    // What is not ours: the restore's own copy, a file of the user's, other shapes.
    CHECK(!is_ours("2026-11-14_11-12-54.zip"));
    CHECK(!is_ours("save.zip") && !is_ours("") && !is_ours(n1 + ".bak"));
    CHECK(!is_ours("2026-11-14_11-12-54_" + std::string(16, 'G') + ".zip"));
    CHECK(!is_ours("2026-11-14_11-12-54_" + std::string(16, 'A') + ".zip"));   // upper case: not ours
    CHECK(!is_ours("2026-11-14T11-12-54_" + tag + ".zip"));

    // The oldest beyond the last three of each user go; the others and what is not ours stay.
    const auto at = [&](const char* date, const char* time, const std::string& t) { return file_name(date, time, t); };
    std::vector<std::string> names = {
        at("2026-11-10", "09:00:00", tag), at("2026-11-11", "09:00:00", tag), at("2026-11-12", "09:00:00", tag),
        at("2026-11-13", "09:00:00", tag), at("2026-11-14", "09:00:00", tag),   // five of the first user
        at("2026-11-12", "10:00:00", other),                                     // one of another
        "2026-11-09_08-00-00.zip", "notes.txt",                                  // not ours
    };
    const auto gone = expired(names, 3);
    CHECK(gone.size() == 2 && gone[0] == at("2026-11-10", "09:00:00", tag) && gone[1] == at("2026-11-11", "09:00:00", tag));
    CHECK(expired(names, 5).empty() && expired({}, 3).empty());
    CHECK(expired(names, 0).size() == 6);   // keeping none: every one of ours, never the others

    // The same day: the time orders them; the list's own order does not matter.
    std::vector<std::string> same = {at("2026-11-14", "12:00:00", tag), at("2026-11-14", "08:00:00", tag), at("2026-11-14", "10:00:00", tag),
                                     at("2026-11-14", "09:30:00", tag)};
    const auto g2 = expired(same, 3);
    CHECK(g2.size() == 1 && g2[0] == at("2026-11-14", "08:00:00", tag));

    CHECK_DONE("local backups: names, what is ours, the oldest beyond the last three of each user passed");
}
