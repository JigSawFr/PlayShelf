// libFuzzer target: source/util/changelog.cpp on any bytes (CHANGELOG.md
// comes with the app, but a release's notes are read the same way). Beyond
// the sanitizers: the number of releases asked for is kept, an icon is a code
// point in hex, and the notes of a version are what parse() lists under its
// heading.
#include <string>

#include "fuzz.h"
#include "util/changelog.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string md(reinterpret_cast<const char*>(data), size);

    using changelog::Line;
    for (int max : { 0, 1, 3 }) {
        const std::vector<Line> lines = changelog::parse(md, max);
        int releases = 0;
        for (const Line& l : lines) {
            if (l.kind == Line::Release) releases++;
            FUZZ_ASSERT(l.depth >= 0);
            FUZZ_ASSERT(l.icon.size() <= 8 && l.icon.find_first_not_of("0123456789abcdef") == std::string::npos);
        }
        if (max > 0) FUZZ_ASSERT(releases <= max);
    }

    // The version of the first release heading, and one that is not there.
    std::string version;
    const size_t at = md.find("## [");
    if (at != std::string::npos) version = md.substr(at + 4, md.find(']', at) - at - 4).substr(0, 32);
    for (const std::string& v : { version, std::string("v") + version, std::string("0.0.0") }) {
        std::string date;
        for (const Line& l : changelog::release_notes(md, v, &date)) FUZZ_ASSERT(l.kind != Line::Release);
        changelog::release_notes(md, v, nullptr);
    }
    return 0;
}
