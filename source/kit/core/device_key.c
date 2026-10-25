// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "device_key.h"

#include <string.h>
#include <switch.h>

// PlayShelf's own sources: any fixed bytes give a key of their own.
static const u8 KEK_SOURCE[16] = {0x50, 0x6c, 0x61, 0x79, 0x53, 0x68, 0x65, 0x6c,
                                  0x66, 0x20, 0x6b, 0x65, 0x6b, 0x20, 0x76, 0x31};   // "PlayShelf kek v1"
static const u8 KEY_SOURCE[16] = {0x50, 0x6c, 0x61, 0x79, 0x53, 0x68, 0x65, 0x6c,
                                  0x66, 0x20, 0x6b, 0x65, 0x79, 0x20, 0x76, 0x31};   // "PlayShelf key v1"

Result device_key(u8 out[16])
{
    Result rc = splCryptoInitialize();
    if (R_FAILED(rc)) return rc;
    u8 kek[16];
    // Option 1: the device-unique key, generation 0.
    rc = splCryptoGenerateAesKek(KEK_SOURCE, 0, 1, kek);
    if (R_SUCCEEDED(rc)) rc = splCryptoGenerateAesKey(kek, KEY_SOURCE, out);
    splCryptoExit();
    memset(kek, 0, sizeof kek);
    return rc;
}
