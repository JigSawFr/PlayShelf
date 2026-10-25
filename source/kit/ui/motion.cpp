// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/motion.hpp"

#include <algorithm>
#include <cmath>

#include "ui/effects.hpp"

namespace motion
{

brls::Time now()
{
    return brls::getCPUTimeUsec();
}

float progress(brls::Time since, float duration_us)
{
    if (since == 0 || duration_us <= 0.0f) return 1.0f;
    const brls::Time at = now();
    if (at <= since) return 0.0f;
    return std::min(1.0f, (float)(at - since) / duration_us);
}

float ease_out(float t)
{
    const float r = 1.0f - std::clamp(t, 0.0f, 1.0f);
    return 1.0f - r * r * r;
}

float ease_in_out(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return 0.5f - 0.5f * std::cos(t * 3.14159265f);
}

float back_out(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    constexpr float c1 = 1.70158f, c3 = c1 + 1.0f;
    const float r = t - 1.0f;
    return 1.0f + c3 * r * r * r + c1 * r * r;
}

namespace
{
// The child that moves its parent: at each frame, the parent's translation
// and alpha from the time elapsed. Drawn as nothing (no size, absolute, not
// culled: its parent may be a line scrolled to the edge).
class Mover : public brls::View
{
  public:
    enum class Kind
    {
        REVEAL,    // from (dy, faint) to (0, alpha)
        DISMISS,   // from (0, alpha) to (-dy, clear)
        SLIDE,     // from (dx, dy) to (0, 0)
        BREATHE,   // alpha down to `low` and back
    };

    Mover(brls::Box* owner, Kind what, float move_x, float move_y, float low_alpha, int ms, int delay_ms)
        : box(owner), kind(what), dx(move_x), dy(move_y), low(low_alpha), duration((float)ms * 1000.0f),
          since(now() + (brls::Time)delay_ms * 1000), rest_alpha(own_alpha(owner))
    {
        this->setFocusable(false);
        this->setPositionType(brls::PositionType::ABSOLUTE);
        this->setWidth(0);
        this->setHeight(0);
        this->setCulled(false);
        this->apply(0.0f);
    }

    void draw(NVGcontext*, float, float, float, float, brls::Style, brls::FrameContext*) override
    {
        if (this->done) return;
        const float t = progress(this->since, this->duration);
        this->apply(t);
        if (t >= 1.0f) this->done = true;
    }

    // Taken off its box (clearViews: a list built again under a line still
    // rising, as Console › Forwarders' cards) or gone with its screen: the
    // movement ends now, or the box stays where it was, nearly clear.
    void willDisappear(bool resetState) override
    {
        brls::View::willDisappear(resetState);
        if (this->done) return;
        this->apply(1.0f);
        this->done = true;
    }

  private:
    // The box's own alpha at rest. getAlpha() is the product down from the
    // root: read inside a screen still rising, it is that screen's near
    // clear, and the box would stay there (About › Help & diagnostics drawn
    // blank, its icons only). A mover still running on the box knows its
    // alpha at rest better than the box, which it has dimmed.
    static float own_alpha(brls::Box* box)
    {
        for (brls::View* child : box->getChildren())
            if (auto* m = dynamic_cast<Mover*>(child); m && !m->done) return m->rest_alpha;
        const float above = box->hasParent() ? box->getParent()->getAlpha() : 1.0f;
        return above > 0.0f ? std::min(1.0f, box->getAlpha() / above) : 1.0f;
    }

    void apply(float t)
    {
        // Nearly clear, not clear: a view at alpha 0 is not drawn at all, and
        // this child with it.
        constexpr float FAINT = 0.02f;
        switch (this->kind) {
            case Kind::REVEAL: {
                const float e = ease_out(t);
                this->box->setTranslationY(this->dy * (1.0f - e));
                this->box->setAlpha(std::max(FAINT, this->rest_alpha * e));
                break;
            }
            case Kind::DISMISS: {
                const float e = ease_out(t);
                this->box->setTranslationY(-this->dy * e);
                this->box->setAlpha(std::max(FAINT, this->rest_alpha * (1.0f - e)));
                break;
            }
            case Kind::SLIDE: {
                const float e = ease_out(t);
                this->box->setTranslationX(this->dx * (1.0f - e));
                this->box->setTranslationY(this->dy * (1.0f - e));
                break;
            }
            case Kind::BREATHE: {
                // Down and back, smoothly: a half cosine.
                const float dip = 0.5f - 0.5f * std::cos(t * 2.0f * 3.14159265f);
                this->box->setAlpha(std::max(FAINT, this->rest_alpha * (1.0f - (1.0f - this->low) * dip)));
                break;
            }
        }
    }

    brls::Box* box;
    Kind kind;
    float dx, dy, low, duration;
    brls::Time since;
    float rest_alpha;
    bool done = false;
};

// Puts a mover on `view`; false when it cannot move (not a box, or the
// motion off).
bool move(brls::View* view, Mover::Kind kind, float dx, float dy, float low, int ms, int delay_ms)
{
    auto* box = dynamic_cast<brls::Box*>(view);
    if (!box || !effects::motion()) return false;
    // The mover first among the children: what it sets holds for the frame
    // its siblings are drawn in.
    box->addView(new Mover(box, kind, dx, dy, low, ms, delay_ms), 0);
    return true;
}
}   // namespace

void reveal(brls::View* view, float rise, int ms, int delay_ms)
{
    move(view, Mover::Kind::REVEAL, 0.0f, rise, 0.0f, ms, delay_ms);
}

void reveal_all(const std::vector<brls::View*>& views, float rise, int ms, int stagger_ms, int limit)
{
    int shown = 0;
    for (brls::View* view : views) {
        if (!view || view->getVisibility() != brls::Visibility::VISIBLE) continue;
        if (shown >= limit) return;
        reveal(view, rise, ms, shown * stagger_ms);
        shown++;
    }
}

void dismiss(brls::View* view, std::function<void()> then, float rise, int ms)
{
    if (!move(view, Mover::Kind::DISMISS, 0.0f, rise, 0.0f, ms, 0)) {
        if (then) then();
        return;
    }
    if (then) brls::delay(ms, then);
}

void slide_from(brls::View* view, float dx, float dy, int ms)
{
    if (std::fabs(dx) < 0.5f && std::fabs(dy) < 0.5f) return;
    move(view, Mover::Kind::SLIDE, dx, dy, 0.0f, ms, 0);
}

void breathe(brls::View* view, float low, int ms)
{
    move(view, Mover::Kind::BREATHE, 0.0f, 0.0f, low, ms, 0);
}

}   // namespace motion
