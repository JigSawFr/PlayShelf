// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "lantern_app/delta.hpp"

#include "lantern_app/file_sink.hpp"

#include <cstdio>
#include <sys/stat.h>

namespace lantern_app
{
namespace
{
uint64_t size_of(const std::string& path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 ? (uint64_t)st.st_size : 0;
}
}   // namespace

bool read_seed(const std::string& path, uint64_t left, std::vector<std::byte>* out)
{
    const uint64_t size = size_of(path);
    if (size == 0 || size > left) return false;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out->resize((size_t)size);
    const bool ok = std::fread(out->data(), 1, out->size(), f) == out->size();
    std::fclose(f);
    return ok;
}

DeltaResult fetch_delta(lantern::Client& client, const PackageRequest& request, const std::string& chunks_url,
                        uint64_t chunks_size, const std::string& chunks_sha256,
                        const std::vector<std::vector<std::byte>>& held, const std::string& path,
                        const std::atomic<bool>* stop, const std::function<void(uint64_t)>& progress)
{
    DeltaResult out;
    const auto  digest = lantern::digest_from_hex(request.sha256);
    const auto  index_digest = lantern::digest_from_hex(chunks_sha256);
    if (!digest || !index_digest) {
        out.whole = true;
        return out;
    }
    std::vector<lantern::Seed> seeds;
    for (const auto& h : held) seeds.emplace_back(h.data(), h.size());
    const lantern::DownloadTarget target{request.url, request.size, *digest};
    lantern::AppChunks            ref;
    ref.url    = chunks_url;
    ref.size   = chunks_size;
    ref.sha256 = *index_digest;
    const auto index = client.chunk_index(ref, target);
    if (!index || !*index) {   // an index that cannot be used: the file whole (L-22.3-4)
        out.whole = true;
        return out;
    }

    FILE* f = open_private(path, false);
    if (!f) {
        out.why = "the file could not be written";
        return out;
    }
    FileSink sink(f, stop, progress, 0);
    const auto done = client.download_with_index(target, **index, seeds, sink);
    std::fclose(f);
    if (done) {
        out.ok = size_of(path) == request.size;
        if (!out.ok) out.why = "the file is not the size the server announced";
        out.from_seeds  = done->from_seeds;
        out.from_ranges = done->from_ranges;
        out.whole       = done->whole_file;
        return out;
    }
    if (stop && stop->load()) {
        out.stopped = true;
        return out;
    }
    out.why = describe(done.error());
    std::remove(path.c_str());
    return out;
}

}   // namespace lantern_app
