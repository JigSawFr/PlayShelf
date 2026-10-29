// library_icons — the covers of a library server's games: each icon read
// from the cache (cache_dir()/library_icons/, what data_cleanup empties),
// else fetched from the server (the credentials and the pin only for the
// server's own address; 1 MB at most) and kept there, then decoded, all off
// the UI thread: the UI thread only makes the pixels a texture of the view
// that shows them (aurore::Image::setImageFromRGBA), freed with it. One
// worker fetches them one after the other, in the order asked. An icon
// neither the server nor the title database has is asked of Nlib API
// (titledb/nlib) when config nlib_api allows it, kept in the same cache; a
// game it does not know is not asked again until PlayShelf starts again.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "action/library_flow.hpp"

namespace aurore
{
class Image;
}

namespace library_icons
{

// An image decoded: width × height RGBA pixels.
struct Pixels
{
    std::vector<uint8_t> rgba;
    int                  width = 0, height = 0;
};
using Picture = std::shared_ptr<const Pixels>;   // nullptr: none
using Done = std::function<void(Picture)>;

// `picture` shown by `image`, a texture of its own (freed with it); false,
// and the image left as it was, when there is none.
bool show(aurore::Image* image, const Picture& picture);

// `done(picture)` on the UI thread: nullptr when there is none (no icon,
// not an image, the server refused it). Not called for an icon dropped
// before it was fetched. `password`: the server's, opened
// (library::password_of). `title_id` (0: none): the game's, whose icon
// local() gives when the server's does not come.
void get(std::shared_ptr<const library_flow::Loaded> l, const std::string& password, const std::string& url, Done done,
         uint64_t title_id = 0);

// A title's icon from the title database (titledb_flow), else from Nlib
// API, as get() gives it (`done(nullptr)`: neither has one).
void local(uint64_t title_id, Done done);

// A picture at `url` (Nlib API's banner, a screenshot: no credentials, no
// pin), from the cache or fetched and kept there like an icon, made at most
// `max_w` × `max_h` (its proportions kept). Not dropped by drop_pending().
void picture(const std::string& url, int max_w, int max_h, Done done);

// An icon from the cache only (one a cover showed before), as get() gives
// it; `done(nullptr)` when it is not there.
void cached(const std::string& url, const Done& done);

// A JPEG or PNG already at hand (a game's icon from the console) decoded on
// aurore::async's queue, `done` on the UI thread (nullptr: not an image).
void decode(std::shared_ptr<const std::vector<uint8_t>> bytes, Done done);

// The icons asked and not fetched yet are not wanted any more (the covers
// were built again); the pictures stay.
void drop_pending();

}   // namespace library_icons
