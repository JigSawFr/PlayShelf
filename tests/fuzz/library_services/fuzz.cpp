// libFuzzer target: source/library/services.cpp's readers on any bytes, an
// AeroFoil server's answer read as its cheats, its save backups and its
// requests. Beyond the sanitizers: every address kept is https://, a build
// id upper-case hex, a title id never 0, every text valid UTF-8 without a
// control character.
#include <string>

#include "fuzz.h"
#include "library/services.hpp"

namespace
{
void text_ok(const std::string& s)
{
    FUZZ_ASSERT(fuzz_utf8_ok(s.data(), s.size()));
    for (char ch : s)
        FUZZ_ASSERT((unsigned char)ch >= 32 && ch != 127);
}
}   // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const std::string body((const char*)data, size);
    const std::string base = "https://shelf.example.org";
    if (const auto list = library::parse_cheats(body, base))
        for (const library::Cheat& c : *list) {
            FUZZ_ASSERT(c.url.compare(0, 8, "https://") == 0 && c.title_id != 0);
            FUZZ_ASSERT(library::normal_build_id(c.build_id) == c.build_id);
            text_ok(c.title_name);
            text_ok(c.note);
        }
    if (const auto list = library::parse_saves(body, base))
        for (const library::SaveBackup& s : *list) {
            FUZZ_ASSERT(s.download_url.compare(0, 8, "https://") == 0 && s.title_id != 0);
            FUZZ_ASSERT(s.delete_url.empty() || s.delete_url.compare(0, 8, "https://") == 0);
            FUZZ_ASSERT(s.note.size() <= library::MAX_NOTE);
            text_ok(s.note);
            text_ok(s.title_name);
        }
    if (const auto list = library::parse_requests(body))
        for (const library::TitleRequest& r : *list) {
            FUZZ_ASSERT(r.title_id != 0);
            text_ok(r.title_name);
            text_ok(r.status);
        }
    return 0;
}
