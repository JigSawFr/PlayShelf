// startup_hook — times Aurore's userAppInit (see util/startup.h): libnx calls
// it before main, and it opens the sockets, the romfs and the network,
// settings and power services, while the screen stays black.
// Linked with -Wl,--wrap=userAppInit (CMakeLists.txt): libnx's call reaches
// __wrap_userAppInit, and __real_userAppInit is Aurore's (switch_wrapper.c).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/startup.h"

void __real_userAppInit(void);

void __wrap_userAppInit(void)
{
    startup_mark(StartupStep_ServicesBegin);
    __real_userAppInit();
    startup_mark(StartupStep_Services);
}
