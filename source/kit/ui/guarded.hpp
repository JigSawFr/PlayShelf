// guarded — a callback that does nothing once its screen is gone.
//
// A screen (a tab, an activity) owns an `alive` token; what finishes later
// (brls::sync after a background task, a flow that waits for the network or
// a dialog) takes guarded(alive, fn) instead of a bare [this]: fn runs only
// while the token is held and true. Checked where the callback is called,
// which is the UI thread: a worker thread hands it to brls::sync, never calls
// it itself. No borealis: host-tested (tests/guarded).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <memory>
#include <utility>

namespace ui
{

// The token a screen's late callbacks check: it goes with the screen (or is
// set to false, for a screen that closes before it is freed).
using Alive = std::shared_ptr<bool>;
// What a background task holds of it: the task never keeps the screen's
// token, it wraps with it the callback it hands to brls::sync.
using Weak = std::weak_ptr<bool>;
inline Alive make_alive() { return std::make_shared<bool>(true); }

// fn(args…), or nothing once `alive` is gone or false. Converts to the
// std::function a flow takes, whatever its arguments.
template <typename F>
auto guarded(Weak alive, F fn)
{
    return [weak = std::move(alive), fn = std::move(fn)](auto&&... args) {
        const Alive held = weak.lock();
        if (!held || !*held) return;
        fn(std::forward<decltype(args)>(args)...);
    };
}
template <typename F>
auto guarded(const Alive& alive, F fn)
{
    return guarded(Weak(alive), std::move(fn));
}

}   // namespace ui
