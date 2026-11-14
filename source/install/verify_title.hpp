// verify_title — an installed title read back and checked: every content its
// record lists is on the console, has the size the record says, and its SHA-256
// is the id it is stored under (an NCA's id is the first 16 bytes of its hash,
// for every package PlayShelf, or any installer that checks, installs). Nothing
// is written. What it finds says what to install again (repair).
// Plain C++ so the host tests can run it (tests/verify_title).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <functional>
#include <vector>

#include "install/installer.hpp"

namespace install
{

enum class ContentState
{
    Ok,
    Missing,      // the record lists it, the console does not have it
    SizeWrong,    // it is there, with another size than the record's
    Unreadable,   // a read failed
    Corrupt,      // read whole, its hash is not its id
};

struct ContentVerdict
{
    ContentId    id{};
    ContentType  type = ContentType::Meta;
    uint64_t     size = 0;   // the record's
    ContentState state = ContentState::Ok;
};

struct TitleVerdict
{
    MetaKey                     key;
    std::vector<ContentVerdict> contents;   // in the record's order, the content meta first

    bool ok() const;
    // The contents to install again: every one that is not Ok.
    std::vector<ContentId> damaged() const;
};

// `progress` gets the bytes read and the total of the contents that exist. `cancel` stops the
// check (an error "cancelled"). An error too when the title is not installed or its record is
// unreadable; a damaged content is a verdict, not an error.
[[nodiscard]] Parsed<TitleVerdict> verify_title(Console& console, const MetaKey& key,
                                                const std::function<void(uint64_t, uint64_t)>& progress = {},
                                                const std::atomic<bool>* cancel = nullptr);

}   // namespace install
