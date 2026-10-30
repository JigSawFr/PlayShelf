// library/news: what the bell tells of Atlas' new games and trends (of a
// series of the console's or the wishlist's games, much like one; the
// wishlist's trends and the console's series'), never a game the console
// has, each told once across runs; and news.json's schema rule (absent,
// newer, unknown keys kept).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <string>

#include "check.h"
#include "library/news.hpp"

using namespace library;

namespace
{
// The console's (OWNED, of the series Q1), the wishlist's (WISHED, of Q2).
constexpr uint64_t OWNED = 0x0100000000010000, WISHED = 0x0100000000020000;
// New on Atlas: of Q1, of Q2, much like OWNED, a little like it, of no
// series, an edition of OWNED, WISHED itself.
constexpr uint64_t IN_Q1 = 0x0100000000030000, IN_Q2 = 0x0100000000040000, ALIKE = 0x0100000000050000,
                   LITTLE = 0x0100000000060000, OTHER = 0x0100000000070000, DELUXE = 0x0100000000080000;

struct Books
{
    SignalBook  signals;
    SimilarBook similar;
    EditionBook editions;
    ChangeBook  changes;
};

void fill(Books& b, const std::string& day)
{
    CHECK(b.signals.from_json(R"({"schema":1,"generated":"2026-10-29","games":{
        "0100000000010000":{"series":[["Q1","Saga"]]},
        "0100000000020000":{"series":[["Q2","Quest"]]},
        "0100000000030000":{"series":[["Q1","Saga"]]},
        "0100000000040000":{"series":[["Q2","Quest"]]},
        "0100000000090000":{"series":[["Q1","Saga"]]}}})"));
    CHECK(b.similar.from_json(R"({"schema":1,"generated":"2026-10-29","games":{
        "0100000000010000":[["0100000000050000",80],["0100000000060000",30]]}})"));
    CHECK(b.editions.from_json(R"({"schema":1,"generated":"2026-10-29","games":{"0100000000080000":"0100000000010000"}})"));
    CHECK(b.changes.from_json(R"({"schema":1,"generated":")" + day + R"(","platforms":{"switch":{
        "new":["0100000000070000","0100000000060000","0100000000050000","0100000000030000","0100000000040000",
               "0100000000080000","0100000000020000","0100000000010000"],
        "trends":[["0100000000090000",20,150],["0100000000020000",10,200],["0100000000010000",0,400],
                  ["0100000000070000",0,900],["0100000000040000",500,100]]}}})"));
}

void test_worth()
{
    Books b;
    fill(b, "2026-11-02");
    const TellBook told;
    const auto     news = news_of(b.changes, b.signals, b.similar, b.editions, { OWNED }, { WISHED }, told);
    if (!CHECK(news.size() == 5)) return;
    // New: a series first (the console's or the wishlist's), then the much
    // alike; not the little alike, nor one of no series, an edition of a
    // game the console has, nor the wishlist's or the console's own.
    CHECK(news[0].kind == NewsKind::New && news[0].id == IN_Q1 && news[0].why == News::Why::Series && news[0].series == "Saga");
    CHECK(news[1].id == IN_Q2 && news[1].series == "Quest");
    CHECK(news[2].id == ALIKE && news[2].why == News::Why::Alike && news[2].like == OWNED);
    // Trends: risen only, the wishlist's first, then a game of the
    // console's series (not one of the wishlist's series alone); not the
    // console's own, nor one of no series.
    CHECK(news[3].kind == NewsKind::Trend && news[3].id == WISHED && news[3].why == News::Why::Wished && news[3].now == 200);
    CHECK(news[4].id == 0x0100000000090000 && news[4].why == News::Why::Series && news[4].series == "Saga" &&
          news[4].before == 20);
    // At most `max` of each kind.
    const auto one = news_of(b.changes, b.signals, b.similar, b.editions, { OWNED }, { WISHED }, told, 1);
    CHECK(one.size() == 2 && one[0].id == IN_Q1 && one[1].id == WISHED);
    // No day, no news.
    CHECK(news_of(ChangeBook{}, b.signals, b.similar, b.editions, { OWNED }, { WISHED }, told).empty());
}

void test_once()
{
    Books b;
    fill(b, "2026-11-02");
    TellBook told;
    for (const News& n : news_of(b.changes, b.signals, b.similar, b.editions, { OWNED }, { WISHED }, told))
        CHECK(told.note(n.kind, n.id, 20261102));
    CHECK(told.size() == 5);
    // Noted once.
    CHECK(!told.note(NewsKind::New, IN_Q1, 20261103));
    // The same run: told still (its entry stays).
    CHECK(news_of(b.changes, b.signals, b.similar, b.editions, { OWNED }, { WISHED }, told).size() == 5);
    // The next run says the same: none told again.
    fill(b, "2026-11-03");
    CHECK(news_of(b.changes, b.signals, b.similar, b.editions, { OWNED }, { WISHED }, told).empty());
    // A game told new may still be told trending, and the other way round.
    CHECK(told.may_tell(NewsKind::Trend, IN_Q1, 20261103) && !told.may_tell(NewsKind::New, IN_Q1, 20261103));
    // A year later, forgotten.
    CHECK(!told.forget_older(20261201) && told.size() == 5);
    CHECK(told.forget_older(20271201) && told.size() == 0);
}

void test_file()
{
    TellBook told;
    told.note(NewsKind::New, IN_Q1, 20261102);
    told.note(NewsKind::Trend, WISHED, 20261103);
    TellBook back;
    CHECK(back.from_json(told.to_json()) && back.size() == 2);
    CHECK(!back.may_tell(NewsKind::New, IN_Q1, 20261103) && back.may_tell(NewsKind::New, IN_Q1, 20261102));
    // Absent: nothing told.
    CHECK(!back.from_json("") && back.size() == 0 && !TellBook::newer(""));
    // Newer: not read, never written over (the caller keeps it).
    const std::string newer = R"({"schema":2,"new":{"0100000000030000":20261102}})";
    CHECK(TellBook::newer(newer) && !back.from_json(newer) && back.size() == 0);
    CHECK(!TellBook::newer(R"({"schema":1})") && !TellBook::newer(R"({"schema":-1})"));
    // An unknown key, read, is written back; bad entries skipped.
    CHECK(back.from_json(R"({"schema":1,"later":{"x":[1,2]},"new":{"0100000000030000":20261102,"zz":20261102,
        "0100000000040000":"soon","0100000000050000":7},"trend":[1]})"));
    CHECK(back.size() == 1);
    const std::string again = back.to_json();
    CHECK(again.find(R"("later":{"x":[1,2]})") != std::string::npos);
    TellBook third;
    CHECK(third.from_json(again) && third.size() == 1 && third.to_json() == again);
    // Bounded: the oldest go first.
    TellBook many;
    for (uint32_t i = 0; i <= TellBook::MAX_GAMES; i++) many.note(NewsKind::New, 0x0100000000000000 + ((uint64_t)(i + 1) << 16), 20260101 + (i == 0 ? 0 : 100));
    CHECK(many.size() == TellBook::MAX_GAMES && many.may_tell(NewsKind::New, 0x0100000000010000, 20261102));
}
}   // namespace

int main()
{
    test_worth();
    test_once();
    test_file();
    CHECK_DONE("library_news");
}
