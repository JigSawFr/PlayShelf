// Host tests for source/util/crash_dump.c: what a crash file says of a CPU
// exception (the kind of fault from the syndrome, a null pointer, addresses
// as offsets into the .nro, the registers) and the frame-pointer walk that
// stops on a damaged stack instead of crashing in the handler.
#include "check.h"
#include <string.h>

#include "crash_dump.h"

#define BASE 0x0000008000000000ULL

static CrashDump dump(uint32_t esr, uint64_t far, uint64_t pc)
{
    CrashDump d;
    memset(&d, 0, sizeof(d));
    d.desc = 0x101;
    d.esr = esr;
    d.far = far;
    d.pc = pc;
    d.lr = BASE + 0x2000;
    d.module_base = BASE;
    d.module_size = 0x400000;
    for (int i = 0; i < 29; i++) d.x[i] = 0x1000u * (uint64_t)i;
    return d;
}

static void test_kinds(void)
{
    CHECK(strcmp(crash_dump_kind(0x101, 0x92000006), "data abort") == 0);
    CHECK(strcmp(crash_dump_kind(0x100, 0x82000006), "instruction abort") == 0);
    CHECK(strcmp(crash_dump_kind(0x101, 0x02000000), "undefined instruction") == 0);
    CHECK(strcmp(crash_dump_kind(0x101, 0xF2000000), "breakpoint") == 0);   // BRK
    CHECK(strcmp(crash_dump_kind(0x103, 0x9A000000), "misaligned stack pointer") == 0);
    // No syndrome: libnx's description.
    CHECK(strcmp(crash_dump_kind(0x102, 0), "misaligned program counter") == 0);
    CHECK(strcmp(crash_dump_kind(0x106, 0), "SError") == 0);
    CHECK(strcmp(crash_dump_kind(0x999, 0), "CPU exception") == 0);
}

static void test_where(void)
{
    CrashDump d = dump(0, 0, 0);
    char out[40];
    crash_dump_where(&d, BASE + 0x1a2b3c, out, sizeof(out));
    CHECK(strcmp(out, "playshelf+0x1a2b3c") == 0);
    // Outside PlayShelf's code (a library of the system, the heap): as is.
    crash_dump_where(&d, BASE + 0x400000, out, sizeof(out));
    CHECK(strcmp(out, "0x0000008000400000") == 0);
    d.module_base = 0;
    crash_dump_where(&d, BASE + 0x10, out, sizeof(out));
    CHECK(strcmp(out, "0x0000008000000010") == 0);
    // Cut, never past the buffer.
    char small[8];
    crash_dump_where(&d, BASE, small, sizeof(small));
    CHECK(strlen(small) == 7);
}

static void test_summary(void)
{
    char out[160];
    // A read of a field of a null pointer.
    CrashDump d = dump(0x92000006, 0x10, BASE + 0x1234);
    crash_dump_summary(&d, out, sizeof(out));
    CHECK(strcmp(out, "data abort reading 0x0000000000000010 (a null pointer) at playshelf+0x1234") == 0);
    // A write (WnR) to unmapped memory.
    d = dump(0x92000046, 0x7700001000ULL, BASE + 0x40);
    crash_dump_summary(&d, out, sizeof(out));
    CHECK(strcmp(out, "data abort writing 0x0000007700001000 (unmapped memory) at playshelf+0x40") == 0);
    // No permission (a write to the code).
    d = dump(0x9200004F, BASE + 0x100, BASE + 0x40);
    crash_dump_summary(&d, out, sizeof(out));
    CHECK(strstr(out, "(no permission)") != NULL);
    // A jump to nowhere.
    d = dump(0x82000006, 0xdead0000ULL, 0xdead0000ULL);
    crash_dump_summary(&d, out, sizeof(out));
    CHECK(strcmp(out, "instruction abort running 0x00000000dead0000 (unmapped memory) at 0x00000000dead0000") == 0);
    // Not an abort: where it happened only.
    d = dump(0x02000000, 0, BASE + 0x99);
    crash_dump_summary(&d, out, sizeof(out));
    CHECK(strcmp(out, "undefined instruction at playshelf+0x99") == 0);
}

static void test_format(void)
{
    CrashDump d = dump(0x92000006, 0x10, BASE + 0x1234);
    d.frames = 2;
    d.frame[0] = BASE + 0x5678;
    d.frame[1] = 0x7100000000ULL;
    char out[4096];
    const size_t len = crash_dump_format(&d, out, sizeof(out));
    CHECK(len == strlen(out));
    CHECK(strstr(out, "cpu exception: data abort reading 0x0000000000000010 (a null pointer) at playshelf+0x1234\n") == out);
    CHECK(strstr(out, "ESR 0x92000006 (class 0x24)"));
    CHECK(strstr(out, "playshelf.nro code at 0x0000008000000000, 0x400000 bytes"));
    CHECK(strstr(out, "  pc   playshelf+0x1234\n  lr   playshelf+0x2000\n  #0   playshelf+0x5678\n  #1   0x0000007100000000\n"));
    CHECK(strstr(out, "  x0  0x0000000000000000  x1  0x0000000000001000  x2  0x0000000000002000\n"));
    CHECK(strstr(out, "  x27 0x000000000001b000  x28 0x000000000001c000\n"));
    CHECK(strstr(out, "  pc  0x0000008000001234\n"));

    // Too small a buffer: cut, NUL-terminated, and the length it needs.
    char small[64];
    CHECK(crash_dump_format(&d, small, sizeof(small)) == len);
    CHECK(strlen(small) == sizeof(small) - 1);
    CHECK(crash_dump_format(&d, NULL, 0) == len);

    // No frame found, no module.
    d.frames = 0;
    d.module_base = 0;
    crash_dump_format(&d, out, sizeof(out));
    CHECK(strstr(out, "(no frame found from the frame pointer)"));
    CHECK(strstr(out, "playshelf.nro code: not found"));
}

// A stack as 8-byte words from STACK; reads elsewhere fail, as unmapped
// memory would.
#define STACK 0x1000ULL
static uint64_t g_stack[64];

static bool read_stack(uint64_t addr, uint64_t *value, void *ctx)
{
    (void)ctx;
    if (addr < STACK || addr + 8 > STACK + sizeof(g_stack) || (addr & 7)) return false;
    *value = g_stack[(addr - STACK) / 8];
    return true;
}

static void frame(uint64_t at, uint64_t next, uint64_t ret)
{
    g_stack[(at - STACK) / 8] = next;
    g_stack[(at - STACK) / 8 + 1] = ret;
}

static void test_backtrace(void)
{
    uint64_t frames[CRASH_DUMP_FRAMES];
    memset(g_stack, 0, sizeof(g_stack));
    // Three frames up the stack, the last one's caller frame pointer 0.
    frame(STACK + 0x10, STACK + 0x40, BASE + 0x100);
    frame(STACK + 0x40, STACK + 0x80, BASE + 0x200);
    frame(STACK + 0x80, 0, BASE + 0x300);
    CHECK(crash_dump_backtrace(STACK + 0x10, read_stack, NULL, frames, CRASH_DUMP_FRAMES) == 3);
    CHECK(frames[0] == BASE + 0x100 && frames[1] == BASE + 0x200 && frames[2] == BASE + 0x300);
    // At most `max`.
    CHECK(crash_dump_backtrace(STACK + 0x10, read_stack, NULL, frames, 2) == 2);

    // A loop (a frame pointing down or at itself) ends the walk.
    frame(STACK + 0x80, STACK + 0x10, BASE + 0x300);
    CHECK(crash_dump_backtrace(STACK + 0x10, read_stack, NULL, frames, CRASH_DUMP_FRAMES) == 3);
    // A frame pointer that cannot be read, or misaligned, or 0: none.
    CHECK(crash_dump_backtrace(0xdead0000ULL, read_stack, NULL, frames, CRASH_DUMP_FRAMES) == 0);
    CHECK(crash_dump_backtrace(STACK + 0x18, read_stack, NULL, frames, CRASH_DUMP_FRAMES) == 0);
    CHECK(crash_dump_backtrace(0, read_stack, NULL, frames, CRASH_DUMP_FRAMES) == 0);
    // A chain that leaves the stack: what was read before stays.
    frame(STACK + 0x40, 0x7fff0000ULL, BASE + 0x200);
    CHECK(crash_dump_backtrace(STACK + 0x10, read_stack, NULL, frames, CRASH_DUMP_FRAMES) == 2);
    // A return address of 0: the end.
    frame(STACK + 0x10, STACK + 0x40, 0);
    CHECK(crash_dump_backtrace(STACK + 0x10, read_stack, NULL, frames, CRASH_DUMP_FRAMES) == 0);
}

int main(void)
{
    test_kinds();
    test_where();
    test_summary();
    test_format();
    test_backtrace();
    return CHECK_DONE("crash dump kinds, summaries, addresses, format and backtrace assertions passed");
}
