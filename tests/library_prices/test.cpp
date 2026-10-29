// library/prices: prices/<COUNTRY>.json read (bad entries skipped, a sale
// above its price no sale), the lowest price so far, the country of a
// language, a price as each country writes it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/prices.hpp"

using namespace library;

namespace
{
constexpr uint64_t SALE = 0x0100000000010000, FULL = 0x0100000000020000, ODD = 0x0100000000030000;

void test_read()
{
    PriceBook book;
    CHECK(book.from_json(R"({"schema":1,"generated":"2026-10-29","country":"FR","currency":"EUR","games":{
        "0100000000010000":[19.99,13.99,20261112],
        "0100000000020000":[59.99,0,0],
        "0100000000030000":[9.99,12.5,20261112],
        "0100000000040000":["19.99",0,0],
        "0100000000050000":[-1,0,0],
        "0100000000060000":[5],
        "0100000000010800":[1,0,0],
        "nothex":[1,0,0]}})"));
    CHECK(book.size() == 3);
    CHECK(book.country() == "FR" && book.generated() == "2026-10-29");
    const Price* s = book.find(SALE);
    if (CHECK(s != nullptr)) {
        CHECK(s->on_sale() && s->discount() == 30 && s->until == 20261112);
    }
    const Price* f = book.find(FULL);
    if (CHECK(f != nullptr)) CHECK(!f->on_sale() && f->discount() == 0);
    // A "sale" dearer than the price: no sale.
    const Price* o = book.find(ODD);
    if (CHECK(o != nullptr)) CHECK(!o->on_sale() && o->until == 0);
    // Not one: another schema, no country of ours, no day.
    PriceBook other;
    CHECK(!other.from_json(R"({"schema":2,"generated":"2026-10-29","country":"FR","games":{}})"));
    CHECK(!other.from_json(R"({"schema":1,"generated":"2026-10-29","country":"XX","games":{}})"));
    CHECK(!other.from_json(R"({"schema":1,"country":"FR","games":{}})"));
    CHECK(!other.from_json("not json"));
    CHECK(other.size() == 0);
}

void test_lowest()
{
    PriceBook book;
    CHECK(book.from_json(R"({"schema":1,"generated":"2026-10-30","country":"FR","since":"2026-10-29","games":{
        "0100000000010000":[19.99,9.99,20261112,9.99,20261030],
        "0100000000020000":[59.99,0,0,39.99,20261029],
        "0100000000030000":[9.99,0,0],
        "0100000000040000":[9.99,0,0,12.5,20261029],
        "0100000000050000":[9.99,0,0,5,"bad"]}})"));
    CHECK(book.since() == "2026-10-29");
    const Price* s = book.find(SALE);
    if (CHECK(s != nullptr)) CHECK(s->lowest == 9.99 && s->lowest_day == 20261030 && s->at_lowest() && s->now() == 9.99);
    const Price* f = book.find(FULL);
    if (CHECK(f != nullptr)) CHECK(f->lowest == 39.99 && !f->at_lowest() && f->now() == 59.99);
    // An old row of three, a lowest above the price, a lowest without its day: none known.
    for (uint64_t id : { ODD, uint64_t(0x0100000000040000), uint64_t(0x0100000000050000) }) {
        const Price* p = book.find(id);
        if (CHECK(p != nullptr)) CHECK(p->lowest == 0 && p->lowest_day == 0 && !p->at_lowest());
    }
    PriceBook old;
    CHECK(old.from_json(R"({"schema":1,"generated":"2026-10-29","country":"FR","games":{}})") && old.since().empty());
}

void test_countries()
{
    CHECK(price_country_of("fr") == "FR");
    CHECK(price_country_of("fr-CA") == "CA");
    CHECK(price_country_of("es-419") == "MX");
    CHECK(price_country_of("pt-BR") == "BR");
    CHECK(price_country_of("ja") == "JP");
    CHECK(price_country_of("ko").empty() && price_country_of("ru").empty() && price_country_of("zh-Hans").empty());
    CHECK(is_price_country("AU") && !is_price_country("") && !is_price_country("fr"));
}

void test_text()
{
    CHECK(price_text(13.99, "FR") == "13,99\xc2\xa0€");
    CHECK(price_text(1299.5, "FR") == "1\xc2\xa0" "299,50\xc2\xa0€");
    CHECK(price_text(13.99, "US") == "$13.99");
    CHECK(price_text(1299.5, "GB") == "£1,299.50");
    CHECK(price_text(1980, "JP") == "¥1,980");
    CHECK(price_text(59.9, "BR") == "R$\xc2\xa0" "59,90");
    CHECK(price_text(59.9, "NL") == "€\xc2\xa0" "59,90");
    CHECK(price_text(10, "XX").empty());
}
}   // namespace

int main()
{
    test_read();
    test_lowest();
    test_countries();
    test_text();
    CHECK_DONE("library_prices");
}
