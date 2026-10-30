// libFuzzer target: source/library/server.cpp on any bytes, a LAN discovery
// answer and a certificate (its DER, its PEM). Beyond the sanitizers: a
// server kept has a UUID, a port and a name of at most 48 bytes; a remote
// address kept is https://.
#include <string>

#include "fuzz.h"
#include "library/server.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (const auto s = library::parse_discovery({(const char*)data, size}, "192.168.1.20")) {
        FUZZ_ASSERT(s->uid.size() == 36 && s->port > 0 && s->name.size() <= 48);
        FUZZ_ASSERT(s->remote.empty() || s->remote.compare(0, 8, "https://") == 0);
    }
    (void)library::spki_sha256({data, size});
    if (const auto der = library::pem_to_der(std::string((const char*)data, size))) (void)library::spki_sha256(*der);
    return 0;
}
