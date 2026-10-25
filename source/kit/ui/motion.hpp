// motion — the small movements of the look, each a few lines where it is
// drawn: a screen or a line that rises into place, a card that bows out, a
// pill that slides to its new place, a chip that breathes once. All of them
// time-based (brls::getCPUTimeUsec: a slow frame skips ahead, a still frame
// draws the end), none of them while effects::motion() says no (the lighter
// effects, Reduced motion, the smoke test's still frames): then every helper
// draws the end state at once, so the screens look the same at rest.
//
// A helper that moves a view adds a child to it (a box, drawn as nothing)
// that advances the view's translation and alpha at each frame and goes
// with the view: nothing to cancel, nothing left dangling when a tab is
// replaced mid-movement.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <functional>
#include <vector>

namespace motion
{
// Now, in µs (the clock every helper reads).
brls::Time now();
// 0..1: how far something begun at `since` (µs) is, over `duration_us`;
// 1 once it is over, or when `since` is 0.
float progress(brls::Time since, float duration_us);
// Easings, 0..1 to 0..1.
float ease_out(float t);     // cubic: fast, then settles
float ease_in_out(float t);  // sine
float back_out(float t);     // overshoots a little, then settles

// `view` rises `rise` px into its place over `ms`, from nearly clear to
// its alpha, after `delay_ms`; nothing (the view as it is) with the
// motion off. For a brls::Box; any other view is left as it is.
void reveal(brls::View* view, float rise = 10.0f, int ms = 180, int delay_ms = 0);
// The same for each of `views` in turn, `stagger_ms` apart, the first
// `limit` of them only (the rest come as they are): a list's lines.
void reveal_all(const std::vector<brls::View*>& views, float rise = 8.0f, int ms = 220, int stagger_ms = 35,
                int limit = 8);
// `view` bows out: up `rise` px and to clear over `ms`, then `then` (what
// hides it, guarded by its caller: the view may be gone by then). With the
// motion off, `then` at once.
void dismiss(brls::View* view, std::function<void()> then, float rise = 8.0f, int ms = 200);
// `view` was just moved by (`dx`, `dy`): it slides from where it was to
// where it is, over `ms`.
void slide_from(brls::View* view, float dx, float dy, int ms = 220);
// `view` breathes once: to `low` alpha and back, over `ms`.
void breathe(brls::View* view, float low = 0.45f, int ms = 700);
}   // namespace motion
