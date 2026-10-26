// library_icons — the covers of a library server's games: each icon read
// from the cache (cache_dir()/library_icons/, what data_cleanup empties),
// else fetched from the server (the credentials and the pin only for the
// server's own address; 1 MB at most) and kept there, then made a texture
// on the UI thread. One worker fetches them one after the other, in the
// order asked. An icon neither the server nor the title database has is
// asked of Nlib API (titledb/nlib) when config online_icons allows it, kept
// in the same cache; a game it does not know is not asked again until
// PlayShelf starts again.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "action/library_flow.hpp"

namespace library_icons
{

// `done(texture)` on the UI thread: an NVG image the caller owns, 0 when
// there is none (no icon, not an image, the server refused it). Not called
// for an icon dropped before it was fetched. `password`: the server's,
// opened (library::password_of). `title_id` (0: none): the game's, whose
// icon local() gives when the server's does not come.
void get(std::shared_ptr<const library_flow::Loaded> l, const std::string& password, const std::string& url,
         std::function<void(int)> done, uint64_t title_id = 0);

// A title's icon from the title database (titledb_flow), else from Nlib
// API, as get() gives it (`done(0)`: neither has one).
void local(uint64_t title_id, std::function<void(int)> done);

// An icon from the cache only (one a cover showed before), as get() gives
// it; `done(0)` when it is not there.
void cached(const std::string& url, const std::function<void(int)>& done);

// The icons asked and not fetched yet are not wanted any more (the covers
// were built again).
void drop_pending();

}   // namespace library_icons
