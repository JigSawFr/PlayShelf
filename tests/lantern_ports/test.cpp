// source/lantern_app/port_data.cpp over a real folder: a port whose data is
// not there, the wrong version, the right one, an optional file left out, and
// an entry that is not a port.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "check.h"
#include "lantern_app/port_data.hpp"

using namespace lantern_app;

namespace
{
const std::string PAK = "frost.pak version one";

void put(const std::string& path, const std::string& bytes)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
}

lantern::Homebrew make_port()
{
    lantern::Homebrew e;
    e.id   = "org.example.frost-port";
    e.kind = "port";
    lantern::Port port;
    port.data_root = "/switch/frost/";
    lantern::RequiredData pak;
    pak.path = "data/frost.pak";
    pak.size = PAK.size();
    pak.hint = "In the folder of the game.";
    lantern::AcceptedVersion v;
    v.sha256 = lantern::Sha256::of(PAK);
    v.label  = "1.0";
    pak.accept.push_back(v);
    port.requires_data.push_back(pak);
    lantern::RequiredData extra;
    extra.path     = "music/extra.ogg";
    extra.optional = true;
    extra.hint     = "From the soundtrack.";
    port.requires_data.push_back(extra);
    e.port = port;
    return e;
}
}   // namespace

int main()
{
    char tmpl[] = "/tmp/lantern_ports_XXXXXX";
    const std::string root = ::mkdtemp(tmpl);
    ::mkdir((root + "/switch").c_str(), 0755);
    ::mkdir((root + "/switch/frost").c_str(), 0755);
    ::mkdir((root + "/switch/frost/data").c_str(), 0755);

    const lantern::Homebrew entry = make_port();

    // Nothing there yet.
    PortData a = check_port_data(entry, root);
    CHECK(a.is_port);
    CHECK(!a.ready);
    CHECK(a.missing.size() == 1);
    if (!a.missing.empty()) CHECK(a.missing[0].find("data/frost.pak") != std::string::npos);
    if (!a.missing.empty()) CHECK(a.missing[0].find("In the folder of the game.") != std::string::npos);

    // A file of another version (same size).
    put(root + "/switch/frost/data/frost.pak", std::string(PAK.size(), 'x'));
    PortData b = check_port_data(entry, root);
    CHECK(!b.ready);
    CHECK(b.missing.size() == 1);

    // The right one; the optional file may stay out.
    put(root + "/switch/frost/data/frost.pak", PAK);
    PortData c = check_port_data(entry, root);
    CHECK(c.ready);
    CHECK(!c.unverified);
    CHECK(c.missing.empty());
    CHECK(c.error.empty());

    // The root given with a trailing slash reads the same.
    CHECK(check_port_data(entry, root + "/").ready);

    // Not a port.
    lantern::Homebrew plain;
    plain.id = "org.example.plain";
    CHECK(!check_port_data(plain, root).is_port);

    std::remove((root + "/switch/frost/data/frost.pak").c_str());
    ::rmdir((root + "/switch/frost/data").c_str());
    ::rmdir((root + "/switch/frost").c_str());
    ::rmdir((root + "/switch").c_str());
    ::rmdir(root.c_str());
    CHECK_DONE("lantern_ports");
}
