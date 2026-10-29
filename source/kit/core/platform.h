// platform — the console services the UI needs besides pctl, time and pdm:
// the focus state, hbloader's "next homebrew", the console region and random
// bytes. The Switch build implements them with
// libnx (core/platform.c); the desktop build in the simulator
// (sim/sim_platform.cpp), where PLAYSHELF_SIM_* variables play the console:
//   PLAYSHELF_SIM_BACKGROUND=1  PlayShelf is not in focus (HOME menu)
//   PLAYSHELF_SIM_HBLOADER=1    started through hbloader: a store can be launched
//   PLAYSHELF_SIM_APPLET=1      applet (album) mode: no game to close on exit
//   PLAYSHELF_SIM_NO_GAME=1     no game open (applet mode with nothing behind)
//   PLAYSHELF_SIM_REGION=2      the console region (SetRegion, 2 = Europe)
//   PLAYSHELF_SIM_BOOT_CHOICE=a the start-up screen's answer (a: A, else B)
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// False while the HOME menu or another applet has the screen.
bool platform_in_focus(void);

// A game is open, so it counts play time and the console would suspend it:
// PlayShelf itself run over a game (title override), or, in applet (album)
// mode, a game left open on the HOME menu. *known false (and true returned)
// when it cannot be read.
bool platform_game_open(bool *known);

// PlayShelf was started through hbloader, which can start another homebrew
// once it exits.
bool platform_can_launch(void);
// hbloader starts `path` (an "sdmc:/…" path; also its argv[0]) when PlayShelf
// exits. False when it cannot.
bool platform_set_next_load(const char *path);
// PlayShelf's exit closes the game it was started over (title override) and
// returns to the HOME menu, rather than to hbloader's next homebrew (hbmenu).
// Nothing in applet (album) mode, where there is no game to close.
void platform_exit_to_home(void);

// romfs:/ (the app's own resources) is read from its .nro, which stays open
// while it is mounted, and the SD card will not rename an open file. Released
// around the replacement of that file (action/dev_build_flow.cpp), then
// mounted again from the file then in place; nothing may read romfs:/ in
// between. Nothing to do on desktop (the resources are plain files there).
void platform_romfs_release(void);
void platform_romfs_mount(void);

// A text screen before Aurore starts (libnx's console), for the
// start-up's own question (action/rollback_flow): `text`, then waits for A
// (true) or B (false; also when the HOME menu closes PlayShelf). Plain
// ASCII: the console's font has nothing else.
bool platform_boot_choice(const char *text);

// The console region (SetRegion: 0 Japan, 1 Americas, 2 Europe,
// 3 Australia / New Zealand, 4 Hong Kong / Taiwan / Korea, 5 China).
bool platform_region(int *region);

// `size` random bytes; false when none could be made.
bool platform_random(void *buf, size_t size);

// PlayShelf's code in memory (the .nro's text): where it starts and its
// size, so a crash file gives addresses as offsets into the .nro. False when
// not known (desktop).
bool platform_code_range(uint64_t *base, uint64_t *size);

// The 8 bytes at `addr` into *value, when they are mapped and readable
// (crash_dump_backtrace's reader; `ctx` unused). False on desktop.
bool platform_read64(uint64_t addr, uint64_t *value, void *ctx);

// The memory PlayShelf has allocated and the memory its process may use, in
// bytes (hbloader gives it what the game or the Album it runs over leaves).
// False when unknown.
bool platform_memory(uint64_t *used, uint64_t *total);

// Ends PlayShelf as a crash the console reports (Atmosphère writes a crash
// report with the stack). abort() would not: on the console it returns to
// hbloader as a normal exit, and the next homebrew (hbmenu) is the one that
// crashes, on a PlayShelf thread left running.
void platform_crash(void);

#ifdef __cplusplus
}
#endif
