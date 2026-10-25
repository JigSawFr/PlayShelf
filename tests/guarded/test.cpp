// Host tests for source/ui/guarded.hpp: the callback runs while its token is
// held and true, with its arguments, and does nothing once the token is gone
// or set to false; it fits a std::function of any arguments.
#include "check.h"
#include <functional>
#include <string>

#include "ui/guarded.hpp"

static void test_runs_while_alive()
{
    ui::Alive alive = ui::make_alive();
    int runs = 0;
    const std::function<void()> f = ui::guarded(alive, [&]() { runs++; });
    f();
    f();
    CHECK(runs == 2);
    alive.reset();   // the screen is freed
    f();
    CHECK(runs == 2);
}

static void test_false_stops_it()
{
    ui::Alive alive = ui::make_alive();
    int runs = 0;
    const auto f = ui::guarded(alive, [&]() { runs++; });
    *alive = false;   // closing, not freed yet
    f();
    CHECK(runs == 0);
}

static void test_arguments()
{
    ui::Alive alive = ui::make_alive();
    std::string got;
    const std::function<void(const std::string&, int)> f =
        ui::guarded(alive, [&](const std::string& s, int n) { got = s + std::to_string(n); });
    f("x", 3);
    CHECK(got == "x3");
    alive.reset();
    f("y", 4);
    CHECK(got == "x3");
}

static void test_copies_share_the_token()
{
    // A copy (brls::sync's queue, a std::function) sees the same token.
    ui::Alive alive = ui::make_alive();
    int runs = 0;
    const auto f = ui::guarded(alive, [&]() { runs++; });
    const auto g = f;
    alive.reset();
    f();
    g();
    CHECK(runs == 0);
}

static void test_from_weak()
{
    // As a background task wraps the callback it hands back: from the weak
    // token it holds, which keeps nothing alive.
    ui::Alive alive = ui::make_alive();
    const ui::Weak weak = alive;
    int runs = 0;
    const auto f = ui::guarded(weak, [&]() { runs++; });
    f();
    CHECK(runs == 1);
    alive.reset();
    CHECK(weak.expired());
    f();
    CHECK(runs == 1);
}

int main()
{
    test_runs_while_alive();
    test_false_stops_it();
    test_arguments();
    test_copies_share_the_token();
    test_from_weak();
    return CHECK_DONE("guarded tests passed");
}
