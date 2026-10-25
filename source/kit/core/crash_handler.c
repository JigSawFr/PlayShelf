// PlayShelf — catches its own CPU exceptions (a bad pointer, an undefined
// instruction, a stack overflow) to write a crash file first
// (util/crash_report.hpp): where it happened, the backtrace, the registers
// and the last lines logged, in sd:/config/playshelf/logs/crashes/.
//
// libnx calls __libnx_exception_handler on the faulting thread, on the stack
// below, when the kernel hands the exception to the process: hbloader passes
// it on to the .nro it runs (nx-hbloader's __libnx_exception_entry). Not
// while a debugger is attached (it gets the exception). When the handler
// returns, libnx ends the process with svcBreak, so Atmosphère still writes
// its own crash report (sd:/atmosphere/crash_reports/) and the console shows
// its error screen, as before.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <switch.h>

#include "platform.h"
#include "util/crash_dump.h"

void crash_report_cpu(const CrashDump *dump);   // util/crash_report.cpp

// libnx's is 1 KiB, too small for the crash file's formatting.
__attribute__((aligned(16))) u8 __nx_exception_stack[0x10000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

static CrashDump g_dump;
static volatile u32 g_inside;

void __libnx_exception_handler(ThreadExceptionDump *ctx)
{
    // A second exception (another thread, or this handler itself): the
    // first one's file is what matters; this one ends as a crash at once.
    if (__atomic_exchange_n(&g_inside, 1, __ATOMIC_ACQ_REL)) return;

    g_dump.desc = ctx->error_desc;
    g_dump.esr = ctx->esr;
    g_dump.far = ctx->far.x;
    for (int i = 0; i < 29; i++) g_dump.x[i] = ctx->cpu_gprs[i].x;
    g_dump.fp = ctx->fp.x;
    g_dump.lr = ctx->lr.x;
    g_dump.sp = ctx->sp.x;
    g_dump.pc = ctx->pc.x;
    g_dump.pstate = ctx->pstate;
    if (!platform_code_range(&g_dump.module_base, &g_dump.module_size)) g_dump.module_base = g_dump.module_size = 0;
    g_dump.frames = crash_dump_backtrace(g_dump.fp, platform_read64, NULL, g_dump.frame, CRASH_DUMP_FRAMES);

    crash_report_cpu(&g_dump);
}
