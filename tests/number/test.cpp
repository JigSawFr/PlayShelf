// Host tests for source/kit/util/number.hpp: a whole text of digits, in base
// 10 or 16; nothing for an empty text, a sign, a space, a stray character or
// a value past the type's range.
#include "check.h"
#include <cstdint>

#include "util/number.hpp"

int main()
{
    CHECK(number::parse<int>("143") == 143);
    CHECK(number::parse<uint32_t>("20261030") == 20261030u);
    CHECK(number::parse<uint64_t>("0100ABCD12345000", 16) == 0x0100ABCD12345000ull);
    CHECK(number::parse<uint64_t>("0100abcd12345000", 16) == 0x0100ABCD12345000ull);
    CHECK(number::parse<int>("007") == 7);

    CHECK(!number::parse<int>(""));
    CHECK(!number::parse<int>("-5") && !number::parse<int>("+5") && !number::parse<int>(" 5"));
    CHECK(!number::parse<int>("5 ") && !number::parse<int>("12a") && !number::parse<int>("0x10", 16));
    CHECK(!number::parse<uint32_t>("4294967296"));   // one past UINT32_MAX
    CHECK(number::parse<uint32_t>("4294967295") == 4294967295u);
    CHECK(!number::parse<uint8_t>("256"));
    return CHECK_DONE("number assertions passed");
}
