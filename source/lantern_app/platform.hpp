// The Lantern client's world on this platform: its transport (libcurl on a
// PC, mbedTLS over the console's sockets on the Switch, none in a browser),
// the trust that transport applies, random bytes, the time, and the
// credentials on the SD card. One per operation: it holds a connection
// and the key it saw, and is not shared between threads.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <memory>
#include <string>

#include "lantern_app/credential_store.hpp"
#include "lantern_app/session.hpp"
#include "lantern_app/system.hpp"

namespace lantern_app
{

class Platform
{
  public:
    Platform();
    Platform(const Platform&)            = delete;
    Platform& operator=(const Platform&) = delete;

    Env&                      env() { return *env_; }
    lantern::CredentialStore& store() { return store_; }
    // Why the transport could not be made ("" when it was).
    const std::string& problem() const { return problem_; }

  private:
    lantern::ServerTrust               trust_;
    SystemRandom                       random_;
    SystemClock                        clock_;
    FileCredentialStore                store_;
    std::string                        ca_;   // the Switch's authorities (PEM)
    std::unique_ptr<lantern::Transport> transport_;
    std::unique_ptr<Env>               env_;
    std::string                        problem_;
};

// "PlayShelf/<version>", as the client names itself to servers.
std::string user_agent();

}   // namespace lantern_app
