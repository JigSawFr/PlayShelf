// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/package.hpp"

#include "lantern_app/delta.hpp"
#include "lantern_app/file_sink.hpp"

#include <array>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

namespace lantern_app
{
namespace
{
uint64_t size_of(const std::string& path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 ? (uint64_t)st.st_size : 0;
}

// The SHA-256 state after the first `have` bytes of the file.
bool hash_prefix(const std::string& path, uint64_t have, lantern::Sha256::State* out)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    lantern::Sha256 sha;
    std::array<std::byte, 64 * 1024> buf;
    uint64_t left = have;
    while (left > 0) {
        const size_t n = std::fread(buf.data(), 1, (size_t)std::min<uint64_t>(left, buf.size()), f);
        if (n == 0) {
            std::fclose(f);
            return false;
        }
        sha.update(std::span<const std::byte>(buf.data(), n));
        left -= n;
    }
    std::fclose(f);
    *out = sha.state();
    return true;
}

std::string origin_of(const std::string& url)
{
    const size_t scheme = url.find("://");
    if (scheme == std::string::npos) return "";
    const size_t slash = url.find('/', scheme + 3);
    return url.substr(0, slash);
}

std::string without_slash(std::string u)
{
    while (!u.empty() && u.back() == '/') u.pop_back();
    return u;
}
}   // namespace

std::unique_ptr<lantern::Client> open_client(Platform& platform, const library::Server& server, const std::string& url,
                                             std::string* why)
{
    // The token goes to the server's own addresses only.
    const std::string origin = origin_of(url);
    const bool        ours = origin == without_slash(origin_of(server.lan_url)) || origin == without_slash(origin_of(server.wan_url));
    if (origin.empty() || !ours || server.uid.empty()) {
        *why = "this package is not on a server this console is paired with";
        return nullptr;
    }

    Env& env = platform.env();
    env.trust.require_ca();
    if (const auto kept = platform.store().load(server.uid, {}); kept && *kept && (*kept)->spki) env.trust.pin(*(*kept)->spki);
    lantern::ClientConfig config;
    config.origin     = origin;
    config.user_agent = env.user_agent;
    config.platform   = env.platform;
    config.random     = &env.random;
    config.clock      = &env.clock;
    config.trust      = &env.trust;
    auto client       = lantern::Client::create(env.transport, config);
    if (!client) {
        *why = describe(client.error());
        return nullptr;
    }
    auto link  = std::make_unique<lantern::Client>(std::move(*client));
    const auto shake = link->handshake();
    if (!shake || !*shake) {
        *why = shake ? "the server speaks a newer version of Lantern than this one reads" : describe(shake.error());
        return nullptr;
    }
    if (std::string((*shake)->uid) != server.uid) {
        *why = "this is another server than the one that was paired";
        return nullptr;
    }
    const auto signed_in = link->sign_in(platform.store());
    if (!signed_in || !*signed_in) {
        *why = signed_in ? "this console is not paired with the server" : describe(signed_in.error());
        return nullptr;
    }
    return link;
}


PackageResult fetch_package(Platform& platform, const library::Server& server, const PackageRequest& request,
                            const std::string& path, const std::atomic<bool>* stop,
                            const std::function<void(uint64_t)>& progress)
{
    PackageResult out;
    const auto digest = lantern::digest_from_hex(request.sha256);
    if (!digest || request.size == 0) {
        out.why = "the catalogue gave no checksum for this package: read it again";
        return out;
    }
    const auto client = open_client(platform, server, request.url, &out.why);
    if (!client) return out;

    // Where the file stands: taken up when it is a prefix of this package.
    lantern::DownloadState state;
    uint64_t               have = size_of(path);
    if (have > request.size || (have > 0 && !hash_prefix(path, have, &state.hash))) {
        std::remove(path.c_str());
        have = 0;
    }
    if (have == 0) state.reset();
    else state.received = have;
    FILE* f = open_private(path, have > 0);
    if (!f) {
        out.why = "the file could not be written";
        return out;
    }
    FileSink sink(f, stop, progress, have);
    const lantern::DownloadTarget target{request.url, request.size, *digest};
    if (have == 0 && !request.peers.empty() && fetch_from_peers(*client, request, target, sink, state, stop, &out.from_peer)) {
        std::fclose(f);
        out.ok = size_of(path) == request.size;
        if (!out.ok) out.why = "the file is not the size the server announced";
        return out;
    }
    if (stop && stop->load()) {
        std::fclose(f);
        out.stopped = true;
        return out;
    }
    const auto done = client->download(target, state, sink);
    std::fclose(f);
    if (done) {
        out.ok = size_of(path) == request.size;
        if (!out.ok) out.why = "the file is not the size the server announced";
        return out;
    }
    if (stop && stop->load()) {
        out.stopped = true;
        return out;
    }
    out.why = describe(done.error());
    if (done.error().category() == lantern::Category::integrity) std::remove(path.c_str());
    return out;
}

DeltaResult fetch_package_delta(Platform& platform, const library::Server& server, const PackageRequest& request,
                                const std::string& chunks_url, uint64_t chunks_size, const std::string& chunks_sha256,
                                const std::vector<std::string>& seed_files, const std::string& path,
                                const std::atomic<bool>* stop, const std::function<void(uint64_t)>& progress)
{
    DeltaResult out;
    const auto  whole = [&]() {
        const PackageResult got = fetch_package(platform, server, request, path, stop, progress);
        static_cast<PackageResult&>(out) = got;
        out.whole = true;
        return out;
    };
    const auto digest = lantern::digest_from_hex(request.sha256);
    const auto index_digest = lantern::digest_from_hex(chunks_sha256);
    if (!digest || request.size == 0 || chunks_url.empty() || !index_digest || chunks_size == 0 || seed_files.empty())
        return whole();

    std::vector<std::vector<std::byte>> held;
    uint64_t                            room = MAX_SEED_BYTES;
    for (const std::string& file : seed_files) {
        std::vector<std::byte> bytes;
        if (!read_seed(file, room, &bytes)) continue;
        room -= bytes.size();
        held.push_back(std::move(bytes));
    }
    if (held.empty()) return whole();
    std::vector<lantern::Seed> seeds;
    for (const auto& h : held) seeds.emplace_back(h.data(), h.size());

    std::string why;
    const auto  client = open_client(platform, server, request.url, &why);
    if (!client) {
        out.why = why;
        return out;
    }
    out = fetch_delta(*client, request, chunks_url, chunks_size, chunks_sha256, held, path, stop, progress, request.peers);
    if (out.whole && !out.ok && !out.stopped && out.why.empty()) return whole();
    return out;
}

}   // namespace lantern_app
