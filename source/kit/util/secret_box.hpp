// secret_box — a secret kept on the SD card only sealed for this console: a
// password, a token. "v1:" and base64 of a random 16-byte nonce, the secret
// encrypted with AES-128-CTR, and an HMAC-SHA256 of "v1", the nonce and the
// ciphertext (encrypt-then-MAC). The two keys are derived from the
// console's own key (core/device_key.h: spl's device-unique key on the
// Switch, a random file on a PC), so a copy of the SD card's files on
// another console or a PC opens nothing; open() refuses a box whose tag
// does not match (another console's, or changed).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace secret_box
{

using Key = std::array<uint8_t, 16>;

// With `key` (the console's) and `nonce` (random; the tests give theirs).
std::string                seal(const std::string& secret, const Key& key, const std::array<uint8_t, 16>& nonce);
std::optional<std::string> open(const std::string& box, const Key& key);
bool                       is_sealed(const std::string& text);   // starts with "v1:"

// With the console's key and a random nonce; nothing when the console's
// key cannot be had (open: nor a box that is not this console's).
std::optional<std::string> seal(const std::string& secret);
std::optional<std::string> open(const std::string& box);

}   // namespace secret_box
