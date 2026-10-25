// Host tests for source/kit/core/write_guard.c: the read-only switch refuses
// every change and every reveal first; the change check refuses with
// NXM_RC_NOT_CONFIRMED; the reveal check stands on its own once set, and
// falls back on the change check when not.
#include "check.h"

#include "write_guard.h"

static int s_asked;
static bool s_answer;

static bool check(void)
{
    s_asked++;
    return s_answer;
}

static bool reveal_no(void)
{
    s_asked++;
    return false;
}

static void test_read_only(void)
{
    CHECK(!core_read_only());
    CHECK(core_change_allowed() == 0 && core_reveal_allowed() == 0);
    core_set_read_only(true);
    CHECK(core_read_only());
    CHECK(core_change_allowed() == NXM_RC_READ_ONLY && core_reveal_allowed() == NXM_RC_READ_ONLY);
    // Read-only is said before anything is asked.
    core_set_change_check(check);
    s_asked = 0;
    CHECK(core_change_allowed() == NXM_RC_READ_ONLY && s_asked == 0);
    core_set_read_only(false);
    core_set_change_check(NULL);
}

static void test_checks(void)
{
    core_set_change_check(check);
    s_answer = false;
    s_asked = 0;
    CHECK(core_change_allowed() == NXM_RC_NOT_CONFIRMED && s_asked == 1);
    // No reveal check: the change check stands in for it.
    CHECK(core_reveal_allowed() == NXM_RC_NOT_CONFIRMED && s_asked == 2);
    s_answer = true;
    CHECK(core_change_allowed() == 0 && core_reveal_allowed() == 0);
    // Its own reveal check, whatever the change check says.
    core_set_reveal_check(reveal_no);
    CHECK(core_change_allowed() == 0 && core_reveal_allowed() == NXM_RC_NOT_CONFIRMED);
    core_set_reveal_check(NULL);
    core_set_change_check(NULL);
    CHECK(core_change_allowed() == 0 && core_reveal_allowed() == 0);
}

int main(void)
{
    test_read_only();
    test_checks();
    return CHECK_DONE("write_guard read-only and check assertions passed");
}
