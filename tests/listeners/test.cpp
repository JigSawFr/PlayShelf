// Host tests for source/kit/util/listeners.hpp: each listener told in the
// order it listened, one gone after unlisten(), and a listener that unlistens
// itself or adds another while told (the next notify() sees the change).
#include "check.h"
#include <string>

#include "util/listeners.hpp"

int main()
{
    Listeners   l;
    std::string told;
    const int   a = l.listen([&] { told += "a"; });
    const int   b = l.listen([&] { told += "b"; });
    CHECK(a != b);
    l.notify();
    CHECK(told == "ab");

    l.unlisten(a);
    l.unlisten(a);   // twice: nothing
    told.clear();
    l.notify();
    CHECK(told == "b");

    // A screen closing as it is told: it goes, the copy still calls it once.
    int self = 0;
    self = l.listen([&] {
        told += "s";
        l.unlisten(self);
        l.listen([&] { told += "n"; });
    });
    told.clear();
    l.notify();
    CHECK(told == "bs");
    told.clear();
    l.notify();
    CHECK(told == "bn");
    return CHECK_DONE("listeners assertions passed");
}
