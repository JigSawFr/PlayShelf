// manifest — a title database's manifest (CyberFoil-DB's
// offline_db_manifest.json): its version and, for each pack, its address,
// size and SHA-256. Its aliases taken as CyberFoil takes them ("version",
// "titles_pack", "path", "sha_256"…); an address relative to the
// manifest's; only HTTPS addresses kept.
//
// Plain C++ for the host tests (tests/titledb).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>

namespace titledb
{

struct PackFile
{
    std::string url, sha256;   // sha256: 64 lowercase hex digits
    uint64_t    size = 0;
    bool        known() const { return !this->url.empty(); }
};

struct Manifest
{
    std::string version;
    PackFile    titles, icons;   // icons: none for a source of names only
};

// False (why set) for a text that is no manifest: not JSON, no version, no
// titles pack, a pack's address not HTTPS, its hash or size missing.
bool parse_manifest(const std::string& text, const std::string& manifest_url, Manifest* out, std::string* why);

// `url` against `base` (the manifest's address): itself when absolute, else
// under base's folder ("/x": base's origin).
std::string resolve(const std::string& base, const std::string& url);

}   // namespace titledb
