// shared_look — the look PlayShelf shares with the other Boréale homebrews
// (sd:/config/boreale/prefs.conf, boreale-link's bl_prefs) while config
// look_shared is on: the language, the theme, the text size and the effects.
// At start the shared values PlayShelf accepts replace its own, and those no
// homebrew set yet are seeded with PlayShelf's; each change PlayShelf saves
// goes there too, for the others' next start. Plain C++ so the host tests
// can run it (tests/shared_look).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <string>

namespace shared_look
{

struct Look
{
    std::string language, theme, text_size, effects;
};

// Whether PlayShelf takes `value` for the key `key` ("language", "theme",
// "text_size", "effects").
using Valid = std::function<bool(const std::string& key, const std::string& value)>;

// The file under the SD card's root `sd_root` ("/" on the console).
std::string file(const std::string& sd_root);

// At start: `look` takes the values of `text` (the file as it is, "" none)
// that `ok` accepts. The file's new text when some keys were not set yet
// (seeded with `look`'s), "" when there is nothing to write or the file is
// a newer version's (never written over).
std::string follow(const std::string& text, Look& look, const Valid& ok);

// After a change PlayShelf saved: the file's text with `look`'s values, the
// keys it does not know kept; "" when nothing changed, or the file is a
// newer version's.
std::string publish(const std::string& text, const Look& look);

}   // namespace shared_look
