// Host tests for source/action/error_hint_logic.cpp: which error codes get a
// hint, and which are shown alone.
#include "check.h"
#include <cstring>

#include "action/error_hint_logic.hpp"
#include "core/nx_types.h"

using error_hint_logic::hint_key;

static bool is(uint32_t rc, const char* key)
{
    return std::strcmp(hint_key(rc), key) == 0;
}

static void test_own()
{
    // Every result PlayShelf defines has its own hint.
    CHECK(is(NXM_RC_READ_ONLY, "read_only"));
    CHECK(is(NXM_RC_NOT_CONFIRMED, "not_confirmed"));
    CHECK(is(NXM_RC_INVALID_ARGUMENT, "invalid_argument"));
    CHECK(is(NXM_RC_FW_UNSUPPORTED, "fw_unsupported"));
    CHECK(is(NXM_RC_NOT_SAVED, "not_saved"));
    CHECK(is(NXM_RC_CANCELLED, "cancelled"));
    // One a newer build would add: nothing, not another one's hint.
    CHECK(is(NXM_RESULT(7), ""));
    CHECK(is(NXM_RESULT(0), ""));
    CHECK(is(NXM_RESULT(0x1FFF), ""));
}

static void test_system()
{
    CHECK(is(0x4B210, "corrupt_data"));   // 2016-0601
    CHECK(is(0x4B410, "corrupt_data"));   // 2016-0602
    CHECK(is(0x4B010, ""));               // 2016-0600: not documented
    CHECK(is(0xF601, "session_closed"));
    CHECK(is((21 | (8 << 9)), "service_denied"));   // 2021-0008
    // Meaning unknown, or not one PlayShelf acts on: the code alone.
    CHECK(is(0x0C15, ""));                // sm InvalidServiceName
    CHECK(is((142 | (100 << 9)), ""));    // a parental-control code
}

int main()
{
    test_own();
    test_system();
    return CHECK_DONE("error_hint_logic assertions passed");
}
