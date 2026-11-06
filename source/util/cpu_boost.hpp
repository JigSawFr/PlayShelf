// cpu_boost — the CPU boost held while loading work runs (an install, a
// compression), counted: the boost goes on with the first holder and off
// with the last, so two overlapping jobs do not switch it off under each
// other. A Guard holds it for a scope, so every way out (done, paused,
// failed) lets go; release_all() is the exit of the app.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace cpu_boost
{
void acquire();
void release();
void release_all();   // the app's exit: the CPU back to normal whoever still holds it
int  holders();

class Guard
{
public:
    // `enabled` false holds nothing (config cpu_boost).
    explicit Guard(bool enabled) : held(enabled) { if (held) acquire(); }
    ~Guard() { if (held) release(); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

private:
    bool held;
};
}   // namespace cpu_boost
