// What the web simulator's page (web/simulator.js) calls while PlayShelf
// runs, on the page's thread, between two frames (web/README.md).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <emscripten.h>

#include <cstring>

#include <borealis.hpp>

#include "core/sim_env.h"

// A PLAYSHELF_SIM_* variable changed without a restart: the ones read at each
// use (web/params.json's `live` entries) take it at their next read. An
// empty or null value removes it. Kept by core/sim_env.h, under its lock:
// the process environment, which other threads read, stays as it was.
extern "C" EMSCRIPTEN_KEEPALIVE int playshelf_web_setenv(const char* name, const char* value)
{
    if (!name || std::strncmp(name, "PLAYSHELF_SIM_", 14) != 0) return 0;
    sim_env_set(name, value);
    return 1;
}

// The CJK font, fetched by the page after PlayShelf started (it is only
// waited for when the page is in Japanese, Korean or Chinese): added behind
// the Latin one, as borealis does at start-up when it is there already.
extern "C" EMSCRIPTEN_KEEPALIVE int playshelf_web_cjk_font(const char* path)
{
    NVGcontext* vg = brls::Application::getNVGContext();
    if (!vg || !path || brls::Application::getFont(brls::FONT_CHINESE_SIMPLIFIED) != brls::FONT_INVALID) return 0;
    if (!brls::Application::loadFontFromFile(brls::FONT_CHINESE_SIMPLIFIED, path)) return 0;
    nvgAddFallbackFontId(vg, brls::Application::getFont(brls::FONT_REGULAR), brls::Application::getFont(brls::FONT_CHINESE_SIMPLIFIED));
    brls::Logger::info("Using {} font: {}", brls::FONT_CHINESE_SIMPLIFIED, path);
    return 1;
}

// The smoke test (tools/web_smoke.py) before a screenshot: 1 while borealis
// holds the input, a screen still opening or closing.
extern "C" EMSCRIPTEN_KEEPALIVE int playshelf_web_busy()
{
    return brls::Application::isInputBlocks() ? 1 : 0;
}
