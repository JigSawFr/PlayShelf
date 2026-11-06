// sim_lantern — the simulators' demo (sim/sim_demo.hpp) as a Lantern server:
// its handshake and its apps (dir()/lantern/, tools/web_demo.cpp's) and its
// packages (dir()/files/), served at DEMO_ORIGIN by a Transport of the
// SDK's, and the pairing that server has already made with this console.
// Nothing leaves the process.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <memory>

#include <lantern/credentials.hpp>
#include <lantern/transport.hpp>

namespace sim_lantern
{

// The demo's uid, as its handshake gives it.
constexpr const char* DEMO_UID = "d0e0d0e0-0000-4000-8000-00000000d0e0";

// Whether this build has the demo's files.
bool available();

// A Transport answering for the demo's origin and refusing every other
// address as a console without a connection would.
std::unique_ptr<lantern::Transport> transport();

// Credentials for the demo's uid, kept in memory.
std::unique_ptr<lantern::CredentialStore> store();

}   // namespace sim_lantern
