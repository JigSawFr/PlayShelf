// library/digits: a count in groups of three digits from the right, from five digits.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/digits.hpp"

using namespace library;

int main()
{
    CHECK(group_digits(0, ",") == "0" && group_digits(214, ",") == "214" && group_digits(1300, ",") == "1300");
    CHECK(group_digits(9999, ",") == "9999" && group_digits(10000, ",") == "10,000" && group_digits(214200, ",") == "214,200");
    CHECK(group_digits(1234567, " ") == "1 234 567" && group_digits(100000, "") == "100000");
    CHECK(group_digits(18446744073709551615ULL, ".") == "18.446.744.073.709.551.615");
    CHECK_DONE("library_digits");
}
