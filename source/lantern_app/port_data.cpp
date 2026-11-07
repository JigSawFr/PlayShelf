// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/port_data.hpp"

#include <cstdio>
#include <optional>
#include <sys/stat.h>

#include "library/catalog.hpp"

namespace lantern_app
{

namespace
{
// The SD card, read at `root` (the paths the library gives are absolute,
// "/switch/x/data.pak": they are put after the root).
class SdProbe final : public lantern::DataProbe
{
  public:
    explicit SdProbe(std::string root) : root_(std::move(root))
    {
        while (!root_.empty() && root_.back() == '/') root_.pop_back();
    }

    lantern::Result<std::optional<std::uint64_t>> size(std::string_view path) override
    {
        struct stat st {};
        if (::stat(full(path).c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return std::optional<std::uint64_t>{};
        return std::optional<std::uint64_t>((std::uint64_t)st.st_size);
    }

    lantern::Result<lantern::Digest> sha256(std::string_view path) override
    {
        FILE* f = std::fopen(full(path).c_str(), "rb");
        if (!f) return std::unexpected(lantern::Error::make(lantern::Category::storage, "the file cannot be opened"));
        lantern::Sha256 hash;
        std::byte       buffer[16384];
        for (;;) {
            const size_t n = std::fread(buffer, 1, sizeof buffer, f);
            if (n > 0) hash.update(std::span<const std::byte>(buffer, n));
            if (n < sizeof buffer) break;
        }
        const bool failed = std::ferror(f) != 0;
        std::fclose(f);
        if (failed) return std::unexpected(lantern::Error::make(lantern::Category::storage, "the file cannot be read"));
        return hash.finish();
    }

  private:
    std::string full(std::string_view path) const { return root_ + std::string(path); }
    std::string root_;
};
}   // namespace

PortData check_port_data(const lantern::Homebrew& entry, const std::string& sd_root)
{
    PortData out;
    if (!entry.port) return out;
    out.is_port = true;
    SdProbe probe(sd_root);
    const auto check = lantern::launch_check(*entry.port, probe);
    if (!check) {
        out.error = library::display_text(std::string(check.error().detail()), 200);
        return out;
    }
    out.ready      = check->ready;
    out.unverified = check->unverified;
    for (const lantern::LaunchIssue& issue : check->issues) {
        std::string line = library::display_text(std::string(issue.path), 128);
        const std::string hint = library::display_text(std::string(issue.hint), 160);
        if (!hint.empty()) line += " (" + hint + ")";
        out.missing.push_back(std::move(line));
    }
    return out;
}

}   // namespace lantern_app
