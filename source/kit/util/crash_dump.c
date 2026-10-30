// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "crash_dump.h"

#include <stdarg.h>
#include <stdio.h>

#include "app_id.h"

// The exception class of the syndrome (ARMv8-A ESR_ELx.EC).
static unsigned ec(uint32_t esr) { return (unsigned)(esr >> 26) & 0x3F; }

enum {
    EC_UNKNOWN = 0x00, EC_FP_ACCESS = 0x07, EC_ILLEGAL = 0x0E, EC_SVC = 0x15, EC_SYSREG = 0x18,
    EC_IABORT = 0x20, EC_PC_ALIGN = 0x22, EC_DABORT = 0x24, EC_SP_ALIGN = 0x26, EC_FP = 0x2C,
    EC_BREAKPOINT = 0x30, EC_STEP = 0x32, EC_WATCHPOINT = 0x34, EC_BRK = 0x3C,
};

// libnx's ThreadExceptionDesc.
enum {
    DESC_IABORT = 0x100, DESC_OTHER = 0x101, DESC_PC_ALIGN = 0x102, DESC_SP_ALIGN = 0x103, DESC_TRAP = 0x104,
    DESC_SERROR = 0x106, DESC_BAD_SVC = 0x301,
};

// Below this, a fault address is a null pointer (or a field of one).
#define NULL_PAGE 0x10000ULL

const char *crash_dump_kind(uint32_t desc, uint32_t esr)
{
    switch (ec(esr)) {
    case EC_DABORT:     return "data abort";
    case EC_IABORT:     return "instruction abort";
    case EC_PC_ALIGN:   return "misaligned program counter";
    case EC_SP_ALIGN:   return "misaligned stack pointer";
    case EC_ILLEGAL:    return "illegal execution state";
    case EC_SVC:        return "bad system call";
    case EC_SYSREG:     return "system register trap";
    case EC_FP_ACCESS:  return "floating-point access trap";
    case EC_FP:         return "floating-point exception";
    case EC_BREAKPOINT: case EC_STEP: case EC_WATCHPOINT: case EC_BRK: return "breakpoint";
    case EC_UNKNOWN:
        // An ESR of 0 is also what an exception without a syndrome leaves.
        if (esr != 0) return "undefined instruction";
        break;
    default: break;
    }
    switch (desc) {
    case DESC_IABORT:   return "instruction abort";
    case DESC_PC_ALIGN: return "misaligned program counter";
    case DESC_SP_ALIGN: return "misaligned stack pointer";
    case DESC_TRAP:     return "trap";
    case DESC_SERROR:   return "SError";
    case DESC_BAD_SVC:  return "bad system call";
    case DESC_OTHER:    return esr == 0 ? "undefined instruction" : "CPU exception";
    default:            return "CPU exception";
    }
}

void crash_dump_where(const CrashDump *d, uint64_t addr, char *out, size_t size)
{
    if (!out || size == 0) return;
    if (d && d->module_base && addr >= d->module_base && addr - d->module_base < d->module_size)
        snprintf(out, size, APP_ID "+0x%llx", (unsigned long long)(addr - d->module_base));
    else
        snprintf(out, size, "0x%016llx", (unsigned long long)addr);
}

// Why a data or instruction abort happened (the fault status code), "" when
// not one of the usual ones.
static const char *fault_status(uint32_t esr)
{
    const unsigned fsc = esr & 0x3F;
    if (fsc >= 0x04 && fsc <= 0x07) return "unmapped memory";
    if (fsc >= 0x09 && fsc <= 0x0B) return "access flag";
    if (fsc >= 0x0D && fsc <= 0x0F) return "no permission";
    if (fsc == 0x21) return "misaligned access";
    return "";
}

void crash_dump_summary(const CrashDump *d, char *out, size_t size)
{
    if (!out || size == 0) return;
    out[0] = '\0';
    if (!d) return;
    char pc[40];
    crash_dump_where(d, d->pc, pc, sizeof(pc));
    const char *kind = crash_dump_kind(d->desc, d->esr);
    const unsigned c = ec(d->esr);
    if (c == EC_DABORT || c == EC_IABORT) {
        // ISS.WnR: a write (data aborts only; instruction aborts are fetches).
        const char *how = c == EC_IABORT ? "running" : (d->esr & (1u << 6)) ? "writing" : "reading";
        const char *why = d->far < NULL_PAGE ? "a null pointer" : fault_status(d->esr);
        snprintf(out, size, "%s %s 0x%016llx%s%s%s at %s", kind, how, (unsigned long long)d->far, *why ? " (" : "",
                 why, *why ? ")" : "", pc);
    } else {
        snprintf(out, size, "%s at %s", kind, pc);
    }
}

// snprintf at the end of what is written so far, keeping count of the whole
// length even past `size`.
typedef struct {
    char  *out;
    size_t size, len;
} Text;

#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
static void add(Text *t, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const size_t room = t->len < t->size ? t->size - t->len : 0;
    const int n = vsnprintf(room ? t->out + t->len : NULL, room, fmt, ap);
    va_end(ap);
    if (n > 0) t->len += (size_t)n;
}

size_t crash_dump_format(const CrashDump *d, char *out, size_t size)
{
    Text t = { out, size, 0 };
    if (out && size) out[0] = '\0';
    if (!d) return 0;
    char where[40], summary[160];
    crash_dump_summary(d, summary, sizeof(summary));
    add(&t, "cpu exception: %s\n", summary);
    add(&t, "  libnx desc 0x%03x, ESR 0x%08x (class 0x%02x), FAR 0x%016llx, PSTATE 0x%08x\n", (unsigned)d->desc,
        (unsigned)d->esr, ec(d->esr), (unsigned long long)d->far, (unsigned)d->pstate);
    if (d->module_base)
        add(&t, "  " APP_ID ".nro code at 0x%016llx, 0x%llx bytes (offsets: the linker map, or addr2line on the .elf)\n",
            (unsigned long long)d->module_base, (unsigned long long)d->module_size);
    else
        add(&t, "  " APP_ID ".nro code: not found (addresses as they are)\n");

    add(&t, "\nbacktrace (innermost first):\n");
    crash_dump_where(d, d->pc, where, sizeof(where));
    add(&t, "  pc   %s\n", where);
    crash_dump_where(d, d->lr, where, sizeof(where));
    add(&t, "  lr   %s\n", where);
    const size_t frames = d->frames < (size_t)CRASH_DUMP_FRAMES ? d->frames : (size_t)CRASH_DUMP_FRAMES;
    for (size_t i = 0; i < frames; i++) {
        crash_dump_where(d, d->frame[i], where, sizeof(where));
        add(&t, "  #%-3u %s\n", (unsigned)i, where);
    }
    if (frames == 0) add(&t, "  (no frame found from the frame pointer)\n");

    add(&t, "\nregisters:\n");
    for (int i = 0; i < 29; i++)
        add(&t, "  x%-2d 0x%016llx%s", i, (unsigned long long)d->x[i], i % 3 == 2 || i == 28 ? "\n" : "");
    add(&t, "  fp  0x%016llx  lr  0x%016llx  sp  0x%016llx\n", (unsigned long long)d->fp, (unsigned long long)d->lr,
        (unsigned long long)d->sp);
    add(&t, "  pc  0x%016llx\n", (unsigned long long)d->pc);
    return t.len;
}

size_t crash_dump_backtrace(uint64_t fp, CrashReadFn read, void *ctx, uint64_t *frames, size_t max)
{
    size_t n = 0;
    if (!read || !frames) return 0;
    while (n < max && fp != 0 && (fp & 0xF) == 0) {
        uint64_t next = 0, ret = 0;
        if (!read(fp, &next, ctx) || !read(fp + 8, &ret, ctx)) break;
        if (ret == 0) break;
        frames[n++] = ret;
        // The caller's frame is higher up the stack: anything else is a loop
        // or a damaged stack.
        if (next <= fp) break;
        fp = next;
    }
    return n;
}
