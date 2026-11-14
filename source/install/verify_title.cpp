// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "install/verify_title.hpp"

#include <algorithm>
#include <cstring>

#include "install/recompress_meta.hpp"
#include "util/sha256.hpp"

namespace install
{

namespace
{
constexpr size_t PIECE = 1u << 20;
}

bool TitleVerdict::ok() const
{
    return std::all_of(contents.begin(), contents.end(), [](const ContentVerdict& c) { return c.state == ContentState::Ok; });
}

std::vector<ContentId> TitleVerdict::damaged() const
{
    std::vector<ContentId> out;
    for (const ContentVerdict& c : contents)
        if (c.state != ContentState::Ok) out.push_back(c.id);
    return out;
}

Parsed<TitleVerdict> verify_title(Console& console, const MetaKey& key, const std::function<void(uint64_t, uint64_t)>& progress,
                                  const std::atomic<bool>* cancel)
{
    const auto record = console.get_meta(key);
    if (!record) return fail("the title is not installed here");
    const auto infos = record_contents(*record);
    if (!infos) return fail(infos.error());

    TitleVerdict verdict;
    verdict.key = key;
    // What exists, with the size the console gives it: the total to read, and the contents that are not there.
    uint64_t total = 0;
    for (size_t i = 0; i < infos->size(); i++) {
        const ContentInfo& c = (*infos)[i];
        ContentVerdict     v;
        v.id = c.id;
        v.type = c.type;
        v.size = c.size;
        if (!console.has_content(c.id)) {
            v.state = ContentState::Missing;
        } else if (const auto size = console.content_size(c.id); !size || *size != c.size) {
            v.state = ContentState::SizeWrong;
        } else {
            total += c.size;
        }
        verdict.contents.push_back(v);
    }

    uint64_t             done = 0;
    std::vector<uint8_t> buffer(PIECE);
    for (size_t i = 0; i < verdict.contents.size(); i++) {
        ContentVerdict& v = verdict.contents[i];
        if (v.state != ContentState::Ok) continue;
        Sha256 sha;
        for (uint64_t at = 0; at < v.size;) {
            if (cancel && cancel->load()) return fail("cancelled");
            const size_t n = (size_t)std::min<uint64_t>(PIECE, v.size - at);
            if (!console.read_content(v.id, at, std::span<uint8_t>(buffer.data(), n))) {
                v.state = ContentState::Unreadable;
                break;
            }
            sha.update(std::span<const uint8_t>(buffer.data(), n));
            at += n;
            done += n;
            if (progress) progress(done, total);
        }
        if (v.state != ContentState::Ok) continue;   // a content cut short: the rest of its bytes are not read
        if (content_id_of(sha.digest()) != v.id) v.state = ContentState::Corrupt;
    }
    if (cancel && cancel->load()) return fail("cancelled");
    return verdict;
}

}   // namespace install
