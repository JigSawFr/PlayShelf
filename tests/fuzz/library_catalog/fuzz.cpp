// libFuzzer target: source/library/catalog.cpp on any bytes, a library
// server's answer, read both as the API's and as a JSON index. Beyond the
// sanitizers: every address kept is https:// and every name non-empty,
// valid UTF-8 and free of control characters; a section's items exist.
#include <string>

#include "fuzz.h"
#include "library/catalog.hpp"

namespace
{
void check(const library::Catalog& c)
{
    for (const library::Item& i : c.items) {
        FUZZ_ASSERT(i.url.compare(0, 8, "https://") == 0);
        FUZZ_ASSERT(i.icon_url.empty() || i.icon_url.compare(0, 8, "https://") == 0);
        FUZZ_ASSERT(!i.name.empty() && fuzz_utf8_ok(i.name.data(), i.name.size()));
        for (char ch : i.name)
            FUZZ_ASSERT((unsigned char)ch >= 32 && ch != 127);
    }
    for (const library::Section& s : c.sections)
        for (size_t i : s.items)
            FUZZ_ASSERT(i < c.items.size());
    for (const std::string& d : c.directories)
        FUZZ_ASSERT(d.compare(0, 8, "https://") == 0);
    FUZZ_ASSERT(c.items.size() <= library::MAX_ITEMS);
}
}   // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string body((const char*)data, size);
    if (const auto c = library::parse_api(body, "https://shelf.example.org:8443/api/remote")) check(*c);
    if (const auto c = library::parse_index(body, "https://files.example.org/lib/index.json")) check(*c);
    (void)library::resolve("https://h.example/a/", body);
    return 0;
}
