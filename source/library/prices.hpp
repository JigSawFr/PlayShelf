// prices — what a game costs on the eShop of one country, from PlayInsights'
// prices/<COUNTRY>.json (schema 1): its regular price and, while it is on
// sale, its sale price and the sale's last day. With the country of each
// language PlayShelf speaks, and a price written as that country writes it.
// Plain C++ for the host tests (tests/library_prices).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace library
{

// The countries PlayInsights has prices for, in the order Preferences lists them.
constexpr const char* PRICE_COUNTRIES[] = { "US", "CA", "MX", "BR", "GB", "FR", "DE", "ES", "IT", "NL", "PT", "AU", "JP" };

struct Price
{
    double   regular = 0;   // in the country's currency
    double   sale = 0;      // 0 when not on sale
    uint32_t until = 0;     // the sale's last day, YYYYMMDD; 0 when unknown

    bool on_sale() const { return this->sale > 0 && this->sale < this->regular; }
    // The discount, in percent (rounded), 0 when not on sale.
    int discount() const;
};

class PriceBook
{
  public:
    static constexpr size_t MAX_GAMES = 30000;
    static constexpr double MAX_PRICE = 1000000;   // above it, an entry is wrong

    // From prices/<COUNTRY>.json; false when it is not one (the book is left empty).
    bool from_json(const std::string& text);
    const Price* find(uint64_t base_id) const;
    size_t size() const { return this->games.size(); }
    const std::string& generated() const { return this->day; }
    const std::string& country() const { return this->where; }

  private:
    std::unordered_map<uint64_t, Price> games;
    std::string                         day;
    std::string                         where;
};

// Whether `code` is one of PRICE_COUNTRIES.
inline bool is_price_country(const std::string& code)
{
    for (const char* c : PRICE_COUNTRIES)
        if (code == c) return true;
    return false;
}
// The country whose prices a language shows ("fr-CA": CA), "" for none.
std::string price_country_of(const std::string& language);
// `amount` as `country` writes it: its currency's sign and decimals.
std::string price_text(double amount, const std::string& country);

}   // namespace library
