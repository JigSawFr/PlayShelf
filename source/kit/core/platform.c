// PlayShelf — console services for the UI, with libnx (see platform.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "platform.h"

#include <malloc.h>
#include <stdio.h>

bool platform_in_focus(void)
{
    return appletGetFocusState() == AppletFocusState_InFocus;
}

bool platform_game_open(bool *known)
{
    if (known) *known = true;
    // Run over a game, PlayShelf is the application that counts.
    if (appletGetAppletType() == AppletType_Application) return true;
    u64 pid = 0;
    if (R_FAILED(pmdmntInitialize())) {
        if (known) *known = false;
        return true;
    }
    const bool open = R_SUCCEEDED(pmdmntGetApplicationProcessId(&pid));
    pmdmntExit();
    return open;
}

bool platform_can_launch(void)
{
    return envHasNextLoad();
}

bool platform_set_next_load(const char *path)
{
    // hbloader passes the argument string as is; like its default
    // ("sdmc:/hbmenu.nro"), argv[0] is the full path of the homebrew.
    return path && envHasNextLoad() && R_SUCCEEDED(envSetNextLoad(path, path));
}

// hbmenu's own exit (+): libnx then ends the application itself (appletExit:
// IApplicationFunctions' exit) instead of going back to hbloader.
extern u32 __nx_applet_exit_mode;

void platform_exit_to_home(void)
{
    if (appletGetAppletType() == AppletType_Application) __nx_applet_exit_mode = 1;
}

bool platform_region(int *region)
{
    SetRegion r;
    if (!region || R_FAILED(setGetRegionCode(&r))) return false;
    *region = (int)r;
    return true;
}

bool platform_boot_choice(const char *text)
{
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    printf("%s", text ? text : "");
    consoleUpdate(NULL);
    bool a = false;
    // Only a new press counts: a button held since the start answers nothing.
    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        if (down & HidNpadButton_A) {
            a = true;
            break;
        }
        if (down & HidNpadButton_B) break;
        consoleUpdate(NULL);
    }
    // The screen goes back to borealis, which sets up its own.
    consoleExit(NULL);
    return a;
}

void platform_romfs_release(void)
{
    romfsExit();
}

void platform_romfs_mount(void)
{
    const Result rc = romfsInit();
    if (R_FAILED(rc)) fprintf(stderr, "platform: romfs not mounted again (0x%X)\n", (unsigned)rc);
}

bool platform_random(void *buf, size_t size)
{
    if (!buf) return false;
    randomGet(buf, size);
    return true;
}

bool platform_code_range(uint64_t *base, uint64_t *size)
{
    // The region this function is in: the .nro's text, which hbloader maps
    // from its first byte (NRO header included), so an offset from it is an
    // address of the linked .elf.
    MemoryInfo mi;
    u32 pi;
    if (!base || !size || R_FAILED(svcQueryMemory(&mi, &pi, (u64)(uintptr_t)&platform_code_range))) return false;
    *base = mi.addr;
    *size = mi.size;
    return true;
}

bool platform_read64(uint64_t addr, uint64_t *value, void *ctx)
{
    (void)ctx;
    MemoryInfo mi;
    u32 pi;
    if (!value || (addr & 7) || R_FAILED(svcQueryMemory(&mi, &pi, addr))) return false;
    if (mi.type == MemType_Unmapped || !(mi.perm & Perm_R) || addr + 8 > mi.addr + mi.size) return false;
    *value = *(const volatile u64 *)(uintptr_t)addr;
    return true;
}

bool platform_memory(uint64_t *used, uint64_t *total)
{
    if (!used || !total) return false;
    // Not InfoType_UsedMemorySize: libnx maps the whole heap at start-up, so
    // the process "uses" all of it from the first frame. What malloc handed
    // out is what PlayShelf really holds.
    *used = (uint64_t)mallinfo().uordblks;
    return R_SUCCEEDED(svcGetInfo(total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0));
}

void platform_crash(void)
{
    diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_ShouldNotHappen));
}
