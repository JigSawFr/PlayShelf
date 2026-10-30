// action/notice_logic: the bell's entries, their kinds and order, those put
// away, which come back when their words change, and the keys kept for the
// next start.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/notice_logic.hpp"
#include "check.h"

using namespace notice_logic;

namespace
{
void test_order()
{
    Store s;
    CHECK(s.set("new_games", Level::Info, "3 new games", "On Home"));
    CHECK(s.set("updates", Level::Action, "2 updates to get", ""));
    CHECK(s.set("download_done", Level::Info, "Aster installed", ""));
    // To do first, then each kind in the order first set.
    const auto v = s.visible();
    CHECK(v.size() == 3 && v[0].id == "updates" && v[1].id == "new_games" && v[2].id == "download_done");
    CHECK(s.action_count() == 1 && s.info_count() == 2);
    // The same again: nothing changed. New words: in its place.
    CHECK(!s.set("updates", Level::Action, "2 updates to get", ""));
    CHECK(s.set("new_games", Level::Info, "4 new games", "On Home"));
    CHECK(s.visible()[1].title == "4 new games");
    CHECK(s.clear("updates") && !s.clear("updates") && s.action_count() == 0);
}

void test_dismiss()
{
    Store s;
    s.set("updates", Level::Action, "2 updates", "");
    s.set("new_games", Level::Info, "3 new games", "");
    // An entry to do stays until its source clears it.
    CHECK(!s.dismiss("updates") && !s.dismiss("nothing"));
    CHECK(s.dismiss("new_games") && !s.dismiss("new_games") && !s.has("new_games") && s.info_count() == 0);
    // The same words: still put away; new ones: told.
    CHECK(!s.set("new_games", Level::Info, "3 new games", ""));
    CHECK(s.set("new_games", Level::Info, "5 new games", "") && s.has("new_games"));
    // Put away, then the next start: still away, until the words change.
    CHECK(s.dismiss("new_games"));
    const auto keys = keys_of(keys_text(s.read_keys()));
    CHECK(keys.size() == 2 && keys[1] == read_key("new_games", "5 new games", ""));
    Store next;
    next.set_read(keys);
    CHECK(!next.set("new_games", Level::Info, "5 new games", "") && next.info_count() == 0);
    CHECK(next.set("new_games", Level::Info, "6 new games", "") && next.info_count() == 1);
    // Cleared by its source: forgotten, told again when set again.
    Store again;
    again.set_read(keys);
    again.set("new_games", Level::Info, "5 new games", "");
    again.clear("new_games");
    CHECK(again.set("new_games", Level::Info, "5 new games", "") && again.has("new_games"));
}

void test_keys()
{
    CHECK(read_key("a", "b", "c") == read_key("a", "b", "c") && read_key("a", "b", "c") != read_key("a", "b", "d"));
    CHECK(read_key("x", "t", "").size() == 10);
    // Lines that are not keys are left out.
    const auto keys = keys_of("ok_one:0123abcd\r\nNOT A KEY\nshort:12\nbad:0123ABCD\n:01234567\nwish.0100:fedcba98");
    CHECK(keys.size() == 2 && keys[0] == "ok_one:0123abcd" && keys[1] == "wish.0100:fedcba98");
    std::string many;
    for (int i = 0; i < 300; i++) many += read_key("k" + std::to_string(i), "", "") + "\n";
    CHECK(keys_of(many).size() == MAX_READ);
    // The file's schema line: written, read past; none (an older file) is 1;
    // a newer one gives nothing, and keys_newer() keeps it from being saved over.
    CHECK(keys_text({ "ok_one:0123abcd" }) == "schema=1\nok_one:0123abcd\n");
    CHECK(keys_of(keys_text({ "ok_one:0123abcd" })).size() == 1 && !keys_newer(keys_text({})) && !keys_newer("ok_one:0123abcd\n"));
    CHECK(keys_newer("schema=2\nok_one:0123abcd\n") && keys_of("schema=2\nok_one:0123abcd\n").empty());
    CHECK(keys_newer("schema=x\n") && !keys_newer("schema=1\r\nok_one:0123abcd"));
}
}   // namespace

int main()
{
    test_order();
    test_dismiss();
    test_keys();
    CHECK_DONE("notice_logic");
}
