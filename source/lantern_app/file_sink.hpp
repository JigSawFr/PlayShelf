// A file the Lantern client writes a download into: appended as it comes,
// emptied when the client starts over, `stop` ending the transfer.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <fcntl.h>
#include <unistd.h>

#include <lantern/download.hpp>

namespace lantern_app
{

// A file opened for writing, readable and writable by its owner only (SD
// card files carry no one else's access): `append` keeps what it holds.
inline FILE* open_private(const std::string& path, bool append)
{
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0600);
    if (fd < 0) return nullptr;
    FILE* f = ::fdopen(fd, append ? "ab" : "wb");
    if (!f) ::close(fd);
    return f;
}

class FileSink final : public lantern::DownloadSink
{
  public:
    FileSink(FILE* f, const std::atomic<bool>* stop, std::function<void(uint64_t)> progress, uint64_t at)
        : file(f), stop(stop), progress(std::move(progress)), written(at)
    {
    }

    lantern::Result<void> write(std::span<const std::byte> bytes) override
    {
        if (stop && stop->load()) return lantern::fail(lantern::Category::usage, "stopped");
        if (std::fwrite(bytes.data(), 1, bytes.size(), file) != bytes.size())
            return lantern::fail(lantern::Category::storage, "the file could not be written (is the SD card full?)");
        written += bytes.size();
        if (progress) progress(written);
        return {};
    }

    lantern::Result<void> restart() override
    {
        std::fflush(file);
        if (::ftruncate(::fileno(file), 0) != 0 || std::fseek(file, 0, SEEK_SET) != 0)
            return lantern::fail(lantern::Category::storage, "the file could not be emptied");
        written = 0;
        if (progress) progress(0);
        return {};
    }

  private:
    FILE*                       file;
    const std::atomic<bool>*    stop;
    std::function<void(uint64_t)> progress;
    uint64_t                    written;
};

}   // namespace lantern_app
