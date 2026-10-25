// Files for the suites that set up a folder and look at what the code left
// in it:
//   fs::write(path, content)  its folders made, 0644 (not fopen's 0666), the
//                             suite stopped when it cannot be written;
//   fs::exists(path)          anything there (a file or a folder);
//   fs::read(path)            as the app reads it (paths::read_file: the path,
//                             or its ".tmp" when the path is missing), "" when
//                             neither is there;
//   fs::content_of(path)      the same, "<none>" when neither is there (told
//                             apart from an empty file).
// fs::read and fs::content_of need source/util/paths.cpp in the suite.
#ifndef PLAYSHELF_TESTS_FS_HPP
#define PLAYSHELF_TESTS_FS_HPP

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "check.h"
#include "util/paths.hpp"

namespace fs
{
inline bool exists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

// mkdir -p, without paths::ensure_dir (a suite may not link paths.cpp).
inline bool make_dirs(const std::string& dir)
{
    for (size_t at = 1; at <= dir.size(); at++) {
        if (at < dir.size() && dir[at] != '/') continue;
        if (mkdir(dir.substr(0, at).c_str(), 0777) != 0 && errno != EEXIST) return false;
    }
    return true;
}

inline void write(const std::string& path, const std::string& content)
{
    const size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0) REQUIRE(make_dirs(path.substr(0, slash)));
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    FILE* f = fd >= 0 ? ::fdopen(fd, "wb") : nullptr;
    REQUIRE(f);
    std::fwrite(content.data(), 1, content.size(), f);
    std::fclose(f);
}

inline std::string read(const std::string& path)
{
    std::string out;
    paths::read_file(path, out);
    return out;
}

inline std::string content_of(const std::string& path)
{
    std::string out;
    return paths::read_file(path, out) ? out : std::string("<none>");
}
}   // namespace fs

#endif
