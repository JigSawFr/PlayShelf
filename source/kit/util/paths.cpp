// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "util/paths.hpp"

#include "app_id.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace paths
{

std::string data_dir()
{
#ifdef __SWITCH__
    return "/config/" APP_ID;
#else
    return "./" APP_ID "_data";
#endif
}

std::string logs_dir()     { return data_dir() + "/logs"; }
std::string reports_dir()  { return logs_dir() + "/reports"; }
std::string crashes_dir()  { return logs_dir() + "/crashes"; }
std::string sessions_dir() { return logs_dir() + "/sessions"; }
std::string uploads_dir()  { return logs_dir() + "/uploads"; }
std::string config_file()  { return data_dir() + "/config.json"; }
std::string themes_dir()   { return data_dir() + "/themes"; }
std::string migrations_file() { return data_dir() + "/migrations.json"; }
std::string cache_dir()    { return data_dir() + "/cache"; }

namespace
{
std::string s_nand = "unknown";
}
void        set_nand(const std::string& key) { s_nand = key.empty() ? "unknown" : key; }
std::string nand_key()     { return s_nand; }
std::string nand_dir()     { return data_dir() + "/nand/" + s_nand; }
std::string history_file() { return nand_dir() + "/history.json"; }
std::string nand_cache_dir() { return nand_dir() + "/cache"; }

std::string sd_root()
{
#ifdef __SWITCH__
    return "/";
#else
    return "./" APP_ID "_data/sd";
#endif
}

bool ensure_dir(const std::string& dir)
{
    // Nearly always there already: one look, not a mkdir per folder of the
    // path (each one on the SD card costs about a millisecond).
    struct stat st;
    if (stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return true;
    std::string partial;
    std::stringstream ss(dir);
    std::string part;
    bool absolute = !dir.empty() && dir[0] == '/';
    if (absolute) partial = "/";
    while (std::getline(ss, part, '/')) {
        if (part.empty()) continue;
        partial += part;
        if (mkdir(partial.c_str(), 0777) != 0 && errno != EEXIST) return false;
        partial += "/";
    }
    return true;
}

bool atomic_write(const std::string& path, const std::string& content, std::string* error)
{
    auto fail = [&](const char* what, int err) {
        if (error) *error = std::string(what) + ": " + std::strerror(err);
        return false;
    };
    std::size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && !ensure_dir(path.substr(0, slash)))
        return fail("Could not create the folder", errno);

    // Written as "<path>.part", then renamed "<path>.tmp" once complete: a
    // ".tmp" is always a whole file, and a write cut short leaves only a
    // ".part" (dropped by the next write), never a ".tmp" taken for the file.
    const std::string tmp = path + ".tmp", part = path + ".part";
    // A previous write stopped between removing the file and renaming the
    // ".tmp": that ".tmp" is the only good copy. Promote it before a newer
    // one takes its name.
    struct stat st;
    if (stat(path.c_str(), &st) != 0 && stat(tmp.c_str(), &st) == 0)
        std::rename(tmp.c_str(), path.c_str());
    // Created 0644 (as util/pt_log.cpp), not fopen's 0666.
    const int fd = ::open(part.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    FILE* f = fd >= 0 ? ::fdopen(fd, "wb") : nullptr;
    if (!f) {
        const int err = errno;
        if (fd >= 0) ::close(fd);
        return fail("Could not open the file", err);
    }
    bool ok = std::fwrite(content.data(), 1, content.size(), f) == content.size();
    int err = ok ? 0 : errno;
    if (ok && std::fflush(f) != 0) { ok = false; err = errno; }
    // On the card before the old file is removed: else a power loss after
    // the rename can leave an empty file in its place.
#ifndef __EMSCRIPTEN__
    if (ok && fsync(fileno(f)) != 0) { ok = false; err = errno; }
#else
    // Not in a browser (web/README.md): there a thread's fsync is a call to
    // the page answered once the page's thread is free again, which it never
    // is while the UI thread waits for this write (ui::save_config_now): both
    // wait for good. Nothing to flush there anyway: the files are in memory,
    // and with keep=1 the page saves them to IndexedDB every 3 s itself.
#endif
    if (std::fclose(f) != 0 && ok) { ok = false; err = errno; }
    if (!ok) {
        std::remove(part.c_str());
        return fail("Could not write the file", err);
    }
    // A ".tmp" next to the file (a write stopped before its last step) is an
    // older copy than this one; FAT rename does not replace it. Without the
    // file (the promotion above failed), that ".tmp" is the only good copy:
    // kept, and on FAT this write fails rather than lose it.
    if (stat(path.c_str(), &st) == 0) std::remove(tmp.c_str());
    if (std::rename(part.c_str(), tmp.c_str()) != 0) {
        const int e = errno;
        std::remove(part.c_str());
        return fail("Could not rename the file", e);
    }
    std::remove(path.c_str());   // FAT (SD card) rename does not replace an existing file
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        // The old file is gone and the new one is complete: keep it, read_file
        // falls back to it.
        return fail("Could not rename the file", errno);
    }
    return true;
}

static bool read_whole(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

// The first `head` bytes, then the last `tail` (all of it when the file is
// no longer): the bytes in between are skipped, not read.
static bool read_ends_of(const std::string& path, size_t head, size_t tail, std::string& out)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    const auto take = [f, &out](size_t count) {
        const size_t at = out.size();
        out.resize(at + count);
        out.resize(at + std::fread(&out[at], 1, count, f));
        return !std::ferror(f);
    };
    bool ok = std::fseek(f, 0, SEEK_END) == 0;
    const long size = ok ? std::ftell(f) : -1;
    ok = size >= 0 && std::fseek(f, 0, SEEK_SET) == 0;
    if (ok && (size_t)size <= head + tail) ok = take((size_t)size);
    else if (ok) ok = take(head) && std::fseek(f, size - (long)tail, SEEK_SET) == 0 && take(tail);
    std::fclose(f);
    return ok;
}

// `read` on `path`, else on its ".tmp": atomic_write stopped between removing
// the old file and renaming the new one (crash, power loss, rename error),
// and the ".tmp" is the complete new content. It is only used when the file
// itself is missing.
template <typename Read> static bool read_or_tmp(const std::string& path, Read read)
{
    if (read(path)) return true;
    struct stat st;
    if (stat(path.c_str(), &st) == 0) return false;
    return read(path + ".tmp");
}

bool read_file(const std::string& path, std::string& out)
{
    return read_or_tmp(path, [&out](const std::string& p) { return read_whole(p, out); });
}

bool read_ends(const std::string& path, size_t head, size_t tail, std::string& out)
{
    return read_or_tmp(path, [&](const std::string& p) { return read_ends_of(p, head, tail, out); });
}

bool file_exists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool exists_or_pending(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 || stat((path + ".tmp").c_str(), &st) == 0;
}

namespace
{
std::string aside_of(const std::string& target) { return target + ".old"; }
}   // namespace

void recover_replace(const std::string& target, const std::string& keep_as)
{
    const std::string aside = aside_of(target);
    if (!file_exists(aside)) return;
    // Stopped after the first step: the target is only aside, put back.
    if (!file_exists(target)) {
        std::rename(aside.c_str(), target.c_str());
        return;
    }
    // Stopped after the second: the new one is in place, the last step done.
    if (!keep_as.empty()) {
        std::remove(keep_as.c_str());   // FAT rename does not replace an existing file
        if (std::rename(aside.c_str(), keep_as.c_str()) == 0) return;
    }
    std::remove(aside.c_str());
}

bool replace_file(const std::string& target, const std::string& fresh, const std::string& keep_as, std::string* error)
{
    auto fail = [error](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    recover_replace(target, keep_as);
    const std::string aside = aside_of(target);
    if (file_exists(aside)) return fail("cannot clear " + aside);
    // FAT has no atomic replace: the old one moves aside first.
    const bool had_target = std::rename(target.c_str(), aside.c_str()) == 0;
    if (!had_target && errno != ENOENT) return fail("cannot move " + target + " aside");
    if (std::rename(fresh.c_str(), target.c_str()) != 0) {
        if (had_target) std::rename(aside.c_str(), target.c_str());
        return fail("cannot put " + fresh + " in place");
    }
    if (had_target && !keep_as.empty()) {
        std::remove(keep_as.c_str());
        if (std::rename(aside.c_str(), keep_as.c_str()) == 0) return true;
    }
    std::remove(aside.c_str());
    return true;
}

bool put_aside(const std::string& path)
{
    struct stat st;
    const std::string bad = path + ".bad", older = bad + ".1";
    if (stat(bad.c_str(), &st) == 0) {
        std::remove(older.c_str());   // FAT rename does not replace an existing file
        if (std::rename(bad.c_str(), older.c_str()) != 0) return false;
    }
    const std::string from = stat(path.c_str(), &st) == 0 ? path : path + ".tmp";
    return std::rename(from.c_str(), bad.c_str()) == 0;
}

std::vector<std::string> list_files(const std::string& dir, const std::string& suffix)
{
    std::vector<std::string> names;
    DIR* d = opendir(dir.c_str());
    if (!d) return names;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n.size() > suffix.size() && n.ends_with(suffix))
            names.push_back(n);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> list_dirs(const std::string& dir)
{
    std::vector<std::string> names;
    DIR* d = opendir(dir.c_str());
    if (!d) return names;
    while (dirent* e = readdir(d)) {
        const std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        struct stat st;
        if (stat((dir + "/" + n).c_str(), &st) == 0 && S_ISDIR(st.st_mode)) names.push_back(n);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    return names;
}

}   // namespace paths
