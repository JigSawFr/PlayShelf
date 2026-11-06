// pairing_flow — Library › pairing this console with a Lantern server
// (SPEC.md § 5): the server gives a code, the person enters it on the
// server's page (or scans its QR code with a phone) and, on a server whose
// certificate no authority vouches for, compares the key it shows with the
// one the server displays. When the server accepts, its token, the device's
// key and the server's pinned key are kept (lantern_app/credential_store)
// and the server's uid is written in servers.json.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>

#include "library/servers.hpp"

namespace pairing_flow
{

// `server` is in servers.json already. `done(true)` once paired; `done(false)`
// when it was cancelled, refused or failed (the person was told).
void start(const library::Server& server, std::function<void(bool paired)> done);

// Forgets this console's credentials for `server` (its token revoked on the
// server's side is not needed: the console no longer holds it) and its uid in
// servers.json, so it pairs again.
void unpair(const library::Server& server);

}   // namespace pairing_flow
