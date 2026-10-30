// rsa — RSA-2048 signature check, PSS with SHA-256 (RFC 8017, MGF1-SHA256,
// a 32-byte salt, e = 65537): how an NCA's header is signed with Nintendo's
// fixed key (nca.hpp). Plain C++ for the host tests and the fuzzers; nothing
// secret goes through it, only public keys and signatures.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace install
{

// Whether `signature` is a valid signature of `msg` under the public key
// whose modulus is `modulus` (both 256 bytes, big-endian). A signature not
// below the modulus is not valid.
bool rsa2048_pss_sha256_verify(std::span<const uint8_t> msg, const uint8_t signature[256], const uint8_t modulus[256]);

}   // namespace install
