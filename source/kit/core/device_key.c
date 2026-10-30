// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "device_key.h"

#include <string.h>
#include <switch.h>

#include "app_id.h"

// The app's own sources ("PlayShelf kek v1", "PlayShelf key v1", the first
// 16 bytes, zeros after a shorter one): any fixed bytes give a key of their
// own, so another app's secrets are not this one's.
static void source_of(const char* text, u8 out[16])
{
    memset(out, 0, 16);
    memcpy(out, text, strnlen(text, 16));
}

Result device_key(u8 out[16])
{
    Result rc = splCryptoInitialize();
    if (R_FAILED(rc)) return rc;
    u8 kek_source[16], key_source[16], kek[16];
    source_of(APP_NAME " kek v1", kek_source);
    source_of(APP_NAME " key v1", key_source);
    // Option 1: the device-unique key, generation 0.
    rc = splCryptoGenerateAesKek(kek_source, 0, 1, kek);
    if (R_SUCCEEDED(rc)) rc = splCryptoGenerateAesKey(kek, key_source, out);
    splCryptoExit();
    memset(kek, 0, sizeof kek);
    return rc;
}
