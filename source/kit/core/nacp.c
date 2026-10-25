// PlayShelf — a game's control data, the service-free part (see nacp.h).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "nacp.h"

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

bool nacp_utf8_valid(const char *s, size_t len)
{
    const unsigned char *p = (const unsigned char *)s;
    for (size_t i = 0; i < len;) {
        const unsigned char c = p[i];
        size_t n;
        u32 cp;
        if (c < 0x80) { i++; continue; }
        if (c >= 0xC2 && c <= 0xDF) { n = 1; cp = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) { n = 2; cp = c & 0x0F; }
        else if (c >= 0xF0 && c <= 0xF4) { n = 3; cp = c & 0x07; }
        else return false;   // a continuation byte first, an overlong 0xC0 / 0xC1, or past U+10FFFF
        if (len - i <= n) return false;
        for (size_t k = 1; k <= n; k++) {
            if ((p[i + k] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (p[i + k] & 0x3F);
        }
        if ((n == 2 && cp < 0x800) || (n == 3 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF))
            return false;
        i += n + 1;
    }
    return true;
}

void nacp_copy_utf8(char *dst, size_t n, const char *src)
{
    size_t len = strnlen(src, n - 1);
    if (len == n - 1)
        while (len > 0 && ((unsigned char)src[len] & 0xC0) == 0x80) len--;   // inside a character
    memcpy(dst, src, len);
    dst[len] = '\0';
}

#define TITLE_SIZE      0x300    // NacpLanguageEntry: name 0x200, publisher 0x100
#define TITLE_NAME      0x200
#define TITLES_PLAIN    16       // entries of the uncompressed block (0x3000 bytes)
#define TITLES_PACKED   32       // what a compressed block inflates to, at most
#define TITLES_BLOCK    0x3000

// Inflates a compressed title block into `out` (TITLES_PACKED entries);
// returns the number of whole entries, 0 when it is broken.
static u32 inflate_titles(const u8 *block, u8 *out)
{
    const u32 packed = (u32)block[0] | ((u32)block[1] << 8);   // little-endian
    if (packed == 0 || packed > TITLES_BLOCK - 2) return 0;
    z_stream z;
    memset(&z, 0, sizeof(z));
    if (inflateInit2(&z, -15) != Z_OK) return 0;   // raw DEFLATE, no zlib header
    z.next_in   = (Bytef *)(block + 2);
    z.avail_in  = packed;
    z.next_out  = out;
    z.avail_out = TITLES_PACKED * TITLE_SIZE;
    const int r = inflate(&z, Z_FINISH);
    const size_t made = TITLES_PACKED * TITLE_SIZE - z.avail_out;
    inflateEnd(&z);
    // The whole stream, or a full buffer (32 entries are all there can be).
    if (r != Z_STREAM_END && z.avail_out != 0) return 0;
    return (u32)(made / TITLE_SIZE);
}

static bool title_used(const u8 *t)
{
    return t[0] != 0 || t[TITLE_NAME] != 0;   // a name or a publisher
}

bool nacp_name(const void *nacp, size_t size, u32 lang, char *out, size_t out_size)
{
    out[0] = '\0';
    if (size < NACP_SIZE) return false;
    const u8 *bytes = (const u8 *)nacp;
    const u8 *titles = bytes;
    u32 count = TITLES_PLAIN;
    u8 *inflated = NULL;
    switch (bytes[NACP_TITLES_FORMAT]) {
        case 0:
            break;
        case 1:
            inflated = (u8 *)malloc(TITLES_PACKED * TITLE_SIZE);
            if (!inflated) return false;
            count  = inflate_titles(bytes, inflated);
            titles = inflated;
            break;
        default:   // a format this code does not know: no guess
            return false;
    }
    const u8 *t = lang < count ? titles + (size_t)lang * TITLE_SIZE : NULL;
    for (u32 i = 0; (!t || !title_used(t)) && i < count; i++) t = titles + (size_t)i * TITLE_SIZE;
    bool ok = false;
    if (t && title_used(t)) {
        const char *name = (const char *)t;
        const size_t len = strnlen(name, TITLE_NAME);
        if (len > 0 && len < TITLE_NAME && nacp_utf8_valid(name, len)) {
            nacp_copy_utf8(out, out_size, name);
            ok = out[0] != '\0';
        }
    }
    free(inflated);
    return ok;
}

bool nacp_rating_ages(const void *nacp, size_t size, int8_t *out)
{
    memset(out, 0xFF, NACP_RATING_COUNT);
    if (!nacp || size < NACP_RATING_AGE + NACP_RATING_COUNT) return false;
    memcpy(out, (const u8 *)nacp + NACP_RATING_AGE, NACP_RATING_COUNT);
    return true;
}
