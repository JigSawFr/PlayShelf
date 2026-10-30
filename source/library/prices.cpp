// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "library/prices.hpp"
#include "library/atlas.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include <aurore/extern/nlohmann/json.hpp>

#include "library/signals.hpp"

namespace library
{

namespace
{
// How a country writes a price: its currency's sign, before or after (a
// no-break space where it is apart), the decimals' separator, the
// thousands' one, how many decimals.
struct Money
{
    const char* country;
    const char* sign;
    bool        after;      // "13,99 €" rather than "€13.99"
    char        decimal;
    const char* thousands;
    int         decimals;
};

constexpr Money MONEY[] = {
    { "US", "$", false, '.', ",", 2 },      { "CA", "$", false, '.', ",", 2 },     { "MX", "$", false, '.', ",", 2 },
    { "BR", "R$\xc2\xa0", false, ',', ".", 2 },    { "GB", "£", false, '.', ",", 2 },     { "FR", "\xc2\xa0€", true, ',', "\xc2\xa0", 2 },
    { "DE", "\xc2\xa0€", true, ',', ".", 2 },      { "ES", "\xc2\xa0€", true, ',', ".", 2 },     { "IT", "\xc2\xa0€", true, ',', ".", 2 },
    { "NL", "€\xc2\xa0", false, ',', ".", 2 },     { "PT", "\xc2\xa0€", true, ',', "\xc2\xa0", 2 },     { "AU", "$", false, '.', ",", 2 },
    { "JP", "¥", false, '.', ",", 0 },
};

const Money* money_of(const std::string& country)
{
    for (const Money& m : MONEY)
        if (country == m.country) return &m;
    return nullptr;
}

uint32_t day_of(const nlohmann::json& v)
{
    if (!v.is_number_integer()) return 0;
    const long long d = v.get<long long>();
    return d >= 20000101 && d <= 21001231 && d % 100 >= 1 && d % 100 <= 31 && d / 100 % 100 >= 1 && d / 100 % 100 <= 12 ? (uint32_t)d : 0;
}
}   // namespace

int Price::discount() const
{
    return this->on_sale() ? (int)std::lround(100.0 * (this->regular - this->sale) / this->regular) : 0;
}

bool PriceBook::from_json(const std::string& text)
{
    this->games.clear();
    this->day.clear();
    this->where.clear();
    this->first.clear();
    const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
    if (!root.is_object() || root.value("schema", 0) != ATLAS_SCHEMA || !root.contains("games") || !root["games"].is_object()) return false;
    const auto generated = root.find("generated");
    if (generated == root.end() || !generated->is_string() || !is_day(generated->get<std::string>())) return false;
    const auto country = root.find("country");
    if (country == root.end() || !country->is_string() || !is_price_country(country->get<std::string>())) return false;
    for (const auto& [key, value] : root["games"].items()) {
        if (this->games.size() >= MAX_GAMES) break;
        const uint64_t id = base_id_of(key);
        if (!id || !value.is_array() || value.size() < 3 || !value[0].is_number() || !value[1].is_number()) continue;
        Price p;
        p.regular = value[0].get<double>();
        p.sale = value[1].get<double>();
        p.until = day_of(value[2]);
        if (!std::isfinite(p.regular) || !std::isfinite(p.sale) || p.regular < 0 || p.regular > MAX_PRICE || p.sale < 0) continue;
        // A sale above the regular price is no sale.
        if (p.sale >= p.regular) p.sale = 0, p.until = 0;
        // The lowest so far, when the file has it: a price, the same or lower than the regular one.
        if (value.size() >= 5 && value[3].is_number() && day_of(value[4])) {
            const double lowest = value[3].get<double>();
            if (std::isfinite(lowest) && lowest > 0 && lowest <= p.regular) p.lowest = lowest, p.lowest_day = day_of(value[4]);
        }
        this->games[id] = p;
    }
    this->day = generated->get<std::string>();
    this->where = country->get<std::string>();
    const auto since = root.find("since");
    if (since != root.end() && since->is_string() && is_day(since->get<std::string>())) this->first = since->get<std::string>();
    return true;
}

const Price* PriceBook::find(uint64_t base_id) const
{
    const auto it = this->games.find(base_id);
    return it == this->games.end() ? nullptr : &it->second;
}

std::string price_country_of(const std::string& language)
{
    static const struct { const char* language; const char* country; } MAP[] = {
        { "fr", "FR" }, { "fr-CA", "CA" }, { "en-US", "US" }, { "en", "US" }, { "de", "DE" }, { "es", "ES" }, { "es-419", "MX" },
        { "it", "IT" }, { "nl", "NL" }, { "pt", "PT" }, { "pt-BR", "BR" }, { "ja", "JP" }, { "en-GB", "GB" },
    };
    for (const auto& m : MAP)
        if (language == m.language) return m.country;
    return "";
}

std::string price_text(double amount, const std::string& country)
{
    const Money* m = money_of(country);
    if (!m || !std::isfinite(amount) || amount < 0) return "";
    const long long minor = std::llround(amount * (m->decimals ? 100 : 1));
    const long long whole = m->decimals ? minor / 100 : minor;
    // The whole part, its thousands apart.
    std::string digits = std::to_string(whole), grouped;
    for (size_t i = 0; i < digits.size(); i++) {
        if (i && (digits.size() - i) % 3 == 0) grouped += m->thousands;
        grouped += digits[i];
    }
    if (m->decimals) {
        char cents[4];
        std::snprintf(cents, sizeof cents, "%02lld", minor % 100);
        grouped += m->decimal;
        grouped += cents;
    }
    return m->after ? grouped + m->sign : m->sign + grouped;
}

}   // namespace library
