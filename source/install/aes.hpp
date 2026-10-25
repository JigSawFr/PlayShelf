// aes — the kit's AES-128 (util/aes.hpp) under install's names.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "util/aes.hpp"

namespace install
{
using aes::Aes128;
using aes::Aes128Ctr;
using aes::aes128_ecb_decrypt;
using aes::aes128_ecb_encrypt;
using aes::aes128_xts_nintendo;
}   // namespace install
