// library/facts: the games' facts kept, written and read back, what does
// not hold together left out, the genres and publishers known.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/facts.hpp"

using namespace library;

namespace
{
Facts aster()
{
    Facts f;
    f.name = "The Ballad of Aster: Wind of the Wilds";
    f.publisher = "Northfox";
    f.release_date = 20170303;
    f.categories = { "Adventure", "Action" };
    f.languages = { "ja", "en", "fr" };
    f.rating = { "Fantasy Violence" };
    f.players = 1;
    f.nsu_id = 70010000000025ull;
    return f;
}

void test_book()
{
    FactsBook b;
    CHECK(!b.find(0x01007EF00011E000) && !b.put(0, aster()));
    CHECK(b.put(0x01007EF00011E000, aster()));
    Facts other;
    other.publisher = "Studio";
    other.categories = { "Racing", "Action" };
    other.demo = true;
    CHECK(b.put(0x0100000000010000, other));
    CHECK(b.size() == 2 && b.find(0x01007EF00011E000)->players == 1);
    // Known again: in place of what was.
    Facts again = aster();
    again.players = 2;
    CHECK(b.put(0x01007EF00011E000, again) && b.size() == 2 && b.find(0x01007EF00011E000)->players == 2);
    CHECK(b.categories() == (std::vector<std::string>{ "Action", "Adventure", "Racing" }));
    CHECK(b.publishers() == (std::vector<std::string>{ "Northfox", "Studio" }));

    // Written, then read back whole.
    FactsBook back;
    CHECK(back.from_json(b.to_json()) && back.size() == 2);
    const Facts* z = back.find(0x01007EF00011E000);
    CHECK(z && z->name == aster().name && z->publisher == "Northfox" && z->release_date == 20170303 && z->players == 2 && z->nsu_id == 70010000000025ull);
    CHECK(z && z->categories == aster().categories && z->languages == aster().languages && z->rating == aster().rating && !z->demo);
    const Facts* s = back.find(0x0100000000010000);
    CHECK(s && s->demo && s->release_date == 0 && s->developer.empty());

    // The ones read at start-up added to those that came meanwhile, which stay.
    FactsBook now;
    Facts     newer = aster();
    newer.players = 8;
    CHECK(now.put(0x01007EF00011E000, newer));
    now.add_missing(back);
    CHECK(now.size() == 2 && now.find(0x01007EF00011E000)->players == 8 && now.find(0x0100000000010000)->demo);
}

void test_read()
{
    FactsBook b;
    // Not the book's: not JSON, another schema, no games.
    CHECK(!b.from_json("{") && !b.from_json(R"({"schema":2,"games":{}})") && !b.from_json(R"({"schema":1})"));
    // A newer one is left alone (action/nlib_flow): newer() says so.
    CHECK(FactsBook::newer(R"({"schema":2,"games":{}})") && !FactsBook::newer(b.to_json()) && !FactsBook::newer("{"));
    CHECK(!FactsBook::newer(R"({"games":{}})") && !FactsBook::newer(R"({"schema":"9"})"));
    // A key that is no title id, an entry that is no object, fields of the
    // wrong kind: what holds together stays.
    CHECK(b.from_json(R"({"schema":1,"games":{
        "0100ZZZZ00010000":{"publisher":"x"},
        "0100000000010000":[1],
        "0100000000020000":{"publisher":5,"players":"two","date":20240101,"categories":["Puzzle",3,""],"demo":"yes"}}})"));
    CHECK(b.size() == 1);
    const Facts* f = b.find(0x0100000000020000);
    CHECK(f && f->publisher.empty() && f->players == 0 && f->release_date == 20240101 && !f->demo);
    CHECK(f && f->categories == (std::vector<std::string>{ "Puzzle" }));
}

void test_dates()
{
    CHECK(date_of("2017-03-03") == 20170303 && date_of("") == 0 && date_of("2017/03/03") == 0 && date_of("20170303") == 0);
}
}   // namespace

int main()
{
    test_book();
    test_read();
    test_dates();
    return CHECK_DONE("library facts: kept, written and read back, broken entries left out, genres and publishers");
}
