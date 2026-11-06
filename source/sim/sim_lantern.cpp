// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "sim/sim_lantern.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

#include "core/sim_env.h"
#include "sim/sim_demo.hpp"

namespace sim_lantern
{
namespace
{
constexpr size_t SLICE = 32u << 10;

bool read_file(const std::string& path, std::string* out)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out->clear();
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out->append(buf, n);
    std::fclose(f);
    return true;
}

bool size_of(const std::string& path, uint64_t* size)
{
    struct stat st;
    if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return false;
    *size = (uint64_t)st.st_size;
    return true;
}

// As long as `n` bytes take at the simulated speed.
void pace(size_t n)
{
    const long long kbs = sim_env_int("PLAYSHELF_SIM_SPEED", 96, 0, 1000000);
    if (kbs > 0) std::this_thread::sleep_for(std::chrono::microseconds((long long)n * 1000000 / (kbs * 1024)));
}

std::string header_of(const lantern::Request& r, std::string_view name)
{
    for (const lantern::Header& h : r.headers)
        if (h.name == name) return std::string(h.value);
    return "";
}

class DemoTransport final : public lantern::Transport
{
  public:
    lantern::Result<void> send(const lantern::Request& r, lantern::ResponseHandler& h) override
    {
        const std::string url(r.url);
        const std::string origin = sim_demo::DEMO_ORIGIN;
        if (!url.starts_with(origin) || sim_demo::dir().empty())
            return lantern::fail(lantern::Category::transport, "simulated: no connection");
        std::string path = url.substr(origin.size());
        if (const size_t q = path.find_first_of("?#"); q != std::string::npos) path.resize(q);
        const std::string dir = sim_demo::dir();
        if (path.contains("..")) return answer(h, 404, "{\"status\": 404}", "application/problem+json");
        std::string body;
        if (path == "/.well-known/lantern") {
            if (!read_file(dir + "/lantern/handshake.json", &body)) return answer(h, 404, "{\"status\": 404}", "application/problem+json");
            return answer(h, 200, body, "application/json");
        }
        if (path == "/api/lantern/v1/apps") {
            if (!read_file(dir + "/lantern/apps.json", &body)) return answer(h, 404, "{\"status\": 404}", "application/problem+json");
            return answer(h, 200, body, "application/json");
        }
        if (path.starts_with("/files/")) return file(r, h, dir + path);
        return answer(h, 404, "{\"status\": 404}", "application/problem+json");
    }

  private:
    static lantern::Result<void> answer(lantern::ResponseHandler& h, uint16_t status, const std::string& body, const char* type)
    {
        if (!h.on_status(status) || !h.on_header("Content-Type", type)) return {};
        const std::span<const std::byte> bytes(reinterpret_cast<const std::byte*>(body.data()), body.size());
        (void)h.on_body(bytes);
        return {};
    }

    // A package, whole or from the byte a Range asks for.
    static lantern::Result<void> file(const lantern::Request& r, lantern::ResponseHandler& h, const std::string& path)
    {
        uint64_t size = 0;
        if (!size_of(path, &size)) return answer(h, 404, "{\"status\": 404}", "application/problem+json");
        uint64_t from = 0;
        bool     ranged = false;
        const std::string range = header_of(r, "Range");
        if (range.starts_with("bytes=")) {
            from = std::strtoull(range.c_str() + 6, nullptr, 10);
            ranged = true;
        }
        if (from >= size && size > 0) return answer(h, 416, "{\"status\": 416}", "application/problem+json");
        if (!h.on_status(ranged ? 206 : 200)) return {};
        if (!h.on_header("Content-Type", "application/octet-stream") || !h.on_header("Accept-Ranges", "bytes") ||
            !h.on_header("Content-Length", std::to_string(size - from)) || !h.on_header("ETag", "\"demo\"")) return {};
        if (ranged &&
            !h.on_header("Content-Range", "bytes " + std::to_string(from) + "-" + std::to_string(size - 1) + "/" + std::to_string(size)))
            return {};
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f || std::fseek(f, (long)from, SEEK_SET) != 0) {
            if (f) std::fclose(f);
            return lantern::fail(lantern::Category::transport, "the file could not be read");
        }
        std::vector<std::byte> buf(SLICE);
        uint64_t left = size - from;
        while (left > 0) {
            const size_t n = std::fread(buf.data(), 1, (size_t)std::min<uint64_t>(SLICE, left), f);
            if (n == 0) break;
            pace(n);
            if (!h.on_body(std::span<const std::byte>(buf.data(), n))) break;
            left -= n;
        }
        std::fclose(f);
        return {};
    }
};

class DemoStore final : public lantern::CredentialStore
{
  public:
    lantern::Result<std::optional<lantern::Credentials>> load(std::string_view uid, lantern::Allocator allocator) override
    {
        if (uid != DEMO_UID) return std::optional<lantern::Credentials>{};
        lantern::Credentials c{allocator};
        c.token = "demo-token";
        return std::optional<lantern::Credentials>{std::move(c)};
    }
    lantern::Result<void> store(std::string_view, const lantern::Credentials&) override { return {}; }
    lantern::Result<void> erase(std::string_view) override { return {}; }
};
}   // namespace

bool available() { return !sim_demo::dir().empty(); }

std::unique_ptr<lantern::Transport> transport() { return std::make_unique<DemoTransport>(); }

std::unique_ptr<lantern::CredentialStore> store() { return std::make_unique<DemoStore>(); }

}   // namespace sim_lantern
