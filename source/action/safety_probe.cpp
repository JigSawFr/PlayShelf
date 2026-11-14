// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/safety_probe.hpp"

#ifdef __SWITCH__
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <switch.h>
#endif

namespace safety_probe
{

safety_logic::Probe run()
{
    safety_logic::Probe probe;
#ifdef __SWITCH__
    // Atmosphere's configuration item that says the emuMMC type (0: none, the real memory).
    if (R_SUCCEEDED(splInitialize())) {
        u64 type = 0;
        if (R_SUCCEEDED(splGetConfig(static_cast<SplConfigItem>(65007), &type)))
            probe.emummc = type != 0 ? safety_logic::Answer::Yes : safety_logic::Answer::No;
        splExit();
    }
    // A few names the console asks for.
    static const char* const names[] = {"conntest.nintendowifi.net", "ctest.cdn.nintendo.net", "atum.hac.lp1.d4c.nintendo.net",
                                        "sun.hac.lp1.d4c.nintendo.net"};
    std::vector<std::vector<safety_logic::Address>> answers;
    for (const char* name : names) {
        std::vector<safety_logic::Address> addresses;
        addrinfo                           hints{};
        hints.ai_family = AF_INET;
        addrinfo* found = nullptr;
        if (getaddrinfo(name, nullptr, &hints, &found) == 0 && found) {
            for (const addrinfo* r = found; r; r = r->ai_next) {
                const auto* in = reinterpret_cast<const sockaddr_in*>(r->ai_addr);
                const auto* ip = reinterpret_cast<const unsigned char*>(&in->sin_addr.s_addr);
                addresses.push_back({ip[0], ip[1], ip[2], ip[3]});
            }
            freeaddrinfo(found);
        }
        answers.push_back(std::move(addresses));
    }
    probe.nintendo_blocked = safety_logic::nintendo_blocked(answers);
#endif
    return probe;
}

}   // namespace safety_probe
