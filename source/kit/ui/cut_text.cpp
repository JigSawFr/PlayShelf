// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/cut_text.hpp"

#include <borealis.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <unistd.h>
#endif

#include "action/frame_stats_flow.hpp"
#include "core/sim_env.h"
#include "ui/ui.hpp"

namespace cut_text
{
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
namespace
{
// In the working directory (the desktop's run folder).
const char* const FILE_NAME = "cut_text.txt";
std::set<std::string> s_seen;

// The labels shown under `view` that are cut; a hidden box hides its own.
void walk(brls::View* view, const std::string& screen, FILE* out)
{
    if (!view || view->getVisibility() != brls::Visibility::VISIBLE) return;
    if (auto* label = dynamic_cast<brls::Label*>(view)) {
        if (label->getWidth() > 0 && label->isTruncated()) {
            const std::string line = screen + "\t" + label->getFullText();
            if (s_seen.insert(line).second) {
                std::fprintf(out, "%s\n", line.c_str());
                std::fflush(out);
            }
        }
        return;
    }
    if (auto* box = dynamic_cast<brls::Box*>(view))
        for (brls::View* child : box->getChildren()) walk(child, screen, out);
}
}   // namespace
#endif

void sample()
{
#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
    static const bool on = sim_env_flag("PLAYSHELF_SIM_CUT_TEXT");
    if (!on) return;
    // A fixed name in the working directory, not a path from the environment
    // (CodeQL: path injection), created with an explicit mode (fopen's
    // depends on the umask).
    static FILE* out = [] {
        const int fd = ::open(FILE_NAME, O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (fd < 0) return static_cast<FILE*>(nullptr);
        FILE* f = ::fdopen(fd, "a");
        if (!f) ::close(fd);
        return f;
    }();
    if (!out) return;
    static auto last = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    if (now - last < std::chrono::seconds(1)) return;
    last = now;
    const auto& stack = brls::Application::getActivitiesStack();
    if (stack.empty()) return;
    // A dialog or a picker keeps the name of the screen it opened over, with
    // what it is after it ("dashboard › dialog").
    brls::View* top = stack.back()->getContentView();
    std::string screen = frame_stats_flow::screen();
    if (dynamic_cast<brls::Dialog*>(top)) screen += " › dialog";
    else if (ui::is_sheet(top)) screen += " › picker";
    walk(top, screen, out);
#endif
}
}   // namespace cut_text
