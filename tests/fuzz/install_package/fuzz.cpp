// libFuzzer target: source/install/package.cpp and Installer::prepare() on
// any bytes, a package from a server, with the suites' test keys and a
// console that has nothing and writes nothing. Beyond the sanitizers: every
// file listed lies within the package; a plan writes no more than the
// contents it lists.
#include <cstring>

#include "fuzz.h"
#include "install/aes.hpp"
#include "install/installer.hpp"
#include "install/package.hpp"

namespace
{
struct Empty : install::Console
{
    bool has_content(const install::ContentId&) override { return false; }
    install::Done create_placeholder(const install::ContentId&, uint64_t) override { abort(); }
    install::Done write_placeholder(const install::ContentId&, uint64_t, const uint8_t*, size_t) override { abort(); }
    install::Done register_placeholder(const install::ContentId&) override { abort(); }
    void delete_placeholder(const install::ContentId&) override {}
    bool has_placeholder(const install::ContentId&) override { return false; }
    std::vector<install::ContentId> placeholders() override { return {}; }
    void delete_content(const install::ContentId&) override {}
    uint64_t free_space() override { return ~0ull; }
    std::optional<std::vector<uint8_t>> get_meta(const install::MetaKey&) override { return std::nullopt; }
    install::Done set_meta(const install::MetaKey&, const std::vector<uint8_t>&) override { abort(); }
    install::Done remove_meta(const install::MetaKey&) override { abort(); }
    install::Done commit_meta() override { abort(); }
    install::Done push_application_record(uint64_t, const install::MetaKey&) override { abort(); }
    install::Done import_ticket(const std::vector<uint8_t>&, const std::vector<uint8_t>&) override { abort(); }
    uint32_t system_version() override { return 0x170001; }
};
}   // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    using namespace install;
    static const uint8_t KAEK[16] = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                                     0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf};
    MemorySource src(std::vector<uint8_t>(data, data + size));
    const auto   pkg = package_open(src);
    if (!pkg) return 0;
    for (const PackageFile& f : pkg->files)
        FUZZ_ASSERT(f.offset <= size && f.size <= size - f.offset);
    NcaKeys keys;
    for (int i = 0; i < 32; i++)
        keys.header_key[i] = (uint8_t)(i + 1);
    keys.unwrap = [](uint8_t generation, uint8_t index, const uint8_t in[16], uint8_t out[16]) {
        if (generation > 4 || index != 0) return false;
        std::memcpy(out, in, 16);
        aes128_ecb_decrypt(KAEK, {out, 16});
        return true;
    };
    Empty     console;
    Installer inst(src, *pkg, console, keys);
    if (inst.prepare()) {
        uint64_t sum = 0;
        for (const TitlePlan& t : inst.plan())
            for (const ContentInfo& c : t.to_write)
                sum += c.size;
        FUZZ_ASSERT(sum == inst.bytes_to_write());
    }
    return 0;
}
