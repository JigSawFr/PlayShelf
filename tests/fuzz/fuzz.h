/* Shared by the libFuzzer targets of tests/fuzz/ (see tests/README.md).
 *
 * FUZZ_ASSERT(expr) is a property the target checks on top of the sanitizers
 * (a bound the code promises, a round trip): when it fails it prints
 * file:line and the expression and aborts, which libFuzzer reports as a crash
 * with the input that caused it. fuzz_utf8_ok() tells whether a string is
 * valid UTF-8 (what util/safe_text.hpp promises of its output). */
#ifndef PLAYSHELF_TESTS_FUZZ_H
#define PLAYSHELF_TESTS_FUZZ_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FUZZ_ASSERT(expr)                                                                   \
    do {                                                                                    \
        if (!(expr)) {                                                                      \
            fprintf(stderr, "%s:%d: fuzz property failed: %s\n", __FILE__, __LINE__, #expr); \
            abort();                                                                        \
        }                                                                                   \
    } while (0)

/* Valid UTF-8: no stray continuation byte, no overlong form, no surrogate,
 * nothing past U+10FFFF, no sequence cut at the end. */
static inline int fuzz_utf8_ok(const char *s, size_t n)
{
    size_t i = 0;
    while (i < n) {
        const unsigned char c = (unsigned char)s[i];
        size_t len;
        uint32_t cp, min;
        if (c < 0x80) { i++; continue; }
        else if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; min = 0x80; }
        else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; min = 0x800; }
        else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; min = 0x10000; }
        else return 0;
        if (i + len > n) return 0;
        for (size_t k = 1; k < len; k++) {
            const unsigned char d = (unsigned char)s[i + k];
            if ((d & 0xC0) != 0x80) return 0;
            cp = (cp << 6) | (d & 0x3F);
        }
        if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
        i += len;
    }
    return 1;
}

#endif
