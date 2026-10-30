// util/secret_box.cpp: a box sealed with a known key and nonce is the one
// another implementation makes (Python's hmac and pyca/cryptography's
// AES-CTR, the scheme written out in the header); it opens with its key
// only, and not once any byte of it changed or it was cut; the console's
// key (the desktop build's file, sim/sim_device_key.cpp) is the same from
// one call to the next, and a box sealed with it opens.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <string>
#include <string_view>

#include "app_id.h"
#include "check.h"
#include "core/device_key.h"
#include "temp_dir.hpp"
#include "util/secret_box.hpp"

namespace
{
const secret_box::Key   KEY = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
const std::array<uint8_t, 16> NONCE = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                       0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
// python3: the same scheme with hmac and pyca/cryptography (see the commit),
// for each app (its APP_ID in the keys' purpose).
const std::string WANT = std::string_view(APP_ID) == "playshelf"
    ? "v1:EBESExQVFhcYGRobHB0eHzn0brSqJ5yDZT1qqcAHWbx15YzOV3BxNLyuym/x/ChxO5CWhbskRkKgfpLreaRBzXZtpw=="
    : std::string_view(APP_ID) == "playguard"
        ? "v1:EBESExQVFhcYGRobHB0eH9NZ5ZSg4/CgfuzIAzQcgdyt62w0Ali7O4KvMtQS+X2Zbc2uoWGnDveFb0Ye6eXA1fvoNA=="
        : "";

void test_known()
{
    const std::string box = secret_box::seal("hunter2: a password", KEY, NONCE);
    CHECK(box == WANT);
    CHECK(secret_box::is_sealed(box));
    CHECK(secret_box::open(box, KEY) == std::string("hunter2: a password"));
    secret_box::Key other = KEY;
    other[15] ^= 1;
    CHECK(!secret_box::open(box, other));
    // Any byte of the base64 changed (to another base64 character), or cut.
    for (size_t i = 3; i < box.size() - 2; i++) {
        std::string bad = box;
        bad[i] = bad[i] == 'A' ? 'B' : 'A';
        CHECK(!secret_box::open(bad, KEY));
    }
    for (size_t n = 0; n < box.size(); n += 7)
        CHECK(!secret_box::open(box.substr(0, n), KEY));
    CHECK(!secret_box::open("hunter2", KEY));
    CHECK(!secret_box::open("v1:not base64!", KEY));
    // An empty secret, a long one.
    CHECK(secret_box::open(secret_box::seal("", KEY, NONCE), KEY) == std::string());
    const std::string long_secret(5000, 'x');
    CHECK(secret_box::open(secret_box::seal(long_secret, KEY, NONCE), KEY) == long_secret);
}

void test_console_key()
{
    TempDir tmp("secret_box", TempDir::Enter);
    u8      a[16], b[16];
    CHECK(device_key(a) == 0 && device_key(b) == 0);
    CHECK(std::equal(a, a + 16, b));
    const auto box = secret_box::seal("ghu_token");
    if (!CHECK(box)) return;
    CHECK(box->compare(0, 3, "v1:") == 0 && box->find("ghu_token") == std::string::npos);
    CHECK(secret_box::open(*box) == std::string("ghu_token"));
    // Two seals of the same secret differ (random nonces).
    CHECK(secret_box::seal("ghu_token") != box);
}
}   // namespace

int main()
{
    test_known();
    test_console_key();
    return CHECK_DONE("secret box: known answer, wrong key, changed and cut boxes, the console's key passed");
}
