// shared_look: the look PlayShelf shares with the other Boréale homebrews.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <string>

#include "action/shared_look.hpp"
#include "check.h"

using shared_look::Look;

static bool has(const std::string& s, const std::string& part)
{
    return s.find(part) != std::string::npos;
}

// PlayShelf's words: no "largest" text size.
static bool ok(const std::string& key, const std::string& value)
{
    return key != "text_size" || value != "largest";
}

int main()
{
    CHECK(shared_look::file("/") == "/config/boreale/prefs.conf");
    CHECK(shared_look::file("./sd") == "./sd/config/boreale/prefs.conf");

    // No file yet: PlayShelf's look seeds it, and stays.
    Look look { "fr", "dark", "auto", "full" };
    std::string text = shared_look::follow("", look, ok);
    CHECK(has(text, "schema=1\n") && has(text, "language=fr\ntheme=dark\ntext_size=auto\neffects=full\n"));
    CHECK(look.language == "fr" && look.effects == "full");

    // Another homebrew's look: taken, but a value PlayShelf does not know
    // is not; the key it had not set is seeded, the unknown one kept.
    look = { "fr", "dark", "normal", "auto" };
    text = shared_look::follow("schema=1\nlanguage=ja\ntheme=light\ntext_size=largest\ndensity=dense\nfuture=1\n", look, ok);
    CHECK(look.language == "ja" && look.theme == "light" && look.text_size == "normal");
    CHECK(has(text, "effects=auto") && has(text, "density=dense") && has(text, "future=1"));
    // Everything set already: nothing to write.
    const std::string full = "schema=1\nlanguage=de\ntheme=system\ntext_size=large\neffects=reduced\n";
    look = { "fr", "dark", "normal", "auto" };
    CHECK(shared_look::follow(full, look, ok).empty() && look.language == "de" && look.effects == "reduced");

    // A newer version's file: read, never written over.
    look = { "fr", "dark", "normal", "auto" };
    CHECK(shared_look::follow("schema=2\ntheme=light\n", look, ok).empty() && look.theme == "light");
    CHECK(shared_look::publish("schema=2\ntheme=light\n", look).empty());

    // A change: written, the others kept; the same again: nothing.
    look = { "de", "light", "large", "reduced" };
    const std::string changed = shared_look::publish(full + "density=dense\n", look);
    CHECK(has(changed, "language=de\ntheme=light\ntext_size=large\neffects=reduced\ndensity=dense\n"));
    CHECK(shared_look::publish(changed, look).empty());
    return CHECK_DONE("shared_look: seeded, followed, published and schema assertions passed");
}
