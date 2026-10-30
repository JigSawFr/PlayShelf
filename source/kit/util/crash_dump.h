// crash_dump — what a crash file says of a CPU exception (a bad pointer, an
// undefined instruction, a stack overflow) the app caught itself
// (core/crash_handler.c): the kind of fault, the address it touched, where
// the code was (the program counter and the return addresses up the stack,
// as offsets into the app's .nro, which the build's linker map or
// `aarch64-none-elf-addr2line -e <app>.elf` turn into functions) and the
// registers.
//
// Plain C with no libnx and no allocation: it runs in the exception handler,
// on a thread that may hold any lock; the host tests feed it made-up dumps.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { CRASH_DUMP_FRAMES = 32 };

typedef struct {
    uint32_t desc;            // libnx's ThreadExceptionDesc (0x100 instruction abort...)
    uint32_t esr;             // the exception syndrome: its class (bits 26-31) and details
    uint64_t far;             // the address a data or instruction abort was about
    uint64_t x[29];           // x0..x28
    uint64_t fp, lr, sp, pc;
    uint32_t pstate;
    uint64_t module_base;     // the app's code in memory; 0 when unknown
    uint64_t module_size;
    size_t   frames;          // return addresses found up the stack (crash_dump_backtrace)
    uint64_t frame[CRASH_DUMP_FRAMES];
} CrashDump;

// "data abort", "instruction abort", "undefined instruction", "stack
// alignment"...: what the syndrome (else libnx's description) says it was.
const char *crash_dump_kind(uint32_t desc, uint32_t esr);

// "<app id>+0x1a2b3c" (APP_ID) when `addr` is in the app's code, else
// "0x0000008000123456". Always NUL-terminated.
void crash_dump_where(const CrashDump *d, uint64_t addr, char *out, size_t size);

// The first line's summary: "data abort reading 0x0000000000000010 (a null
// pointer) at <app id>+0x1a2b3c". Always NUL-terminated.
void crash_dump_summary(const CrashDump *d, char *out, size_t size);

// The whole section: the fault, the program counter, the return address,
// the backtrace and the registers. Always NUL-terminated; returns the length
// the whole text needs (snprintf-like).
size_t crash_dump_format(const CrashDump *d, char *out, size_t size);

// Reads the 8 bytes at `addr` into *value when they are mapped and readable.
typedef bool (*CrashReadFn)(uint64_t addr, uint64_t *value, void *ctx);

// Follows the frame-pointer chain from `fp` (each frame: the caller's frame
// pointer, then the return address), at most `max` return addresses into
// `frames`. Stops at a frame pointer that is 0, misaligned, unreadable or not
// above the one before (the stack grows down): a damaged stack ends the
// list, it never crashes it. Returns how many were found.
size_t crash_dump_backtrace(uint64_t fp, CrashReadFn read, void *ctx, uint64_t *frames, size_t max);

#ifdef __cplusplus
}
#endif
