// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "saves/archive.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

namespace save_archive
{
namespace
{
constexpr size_t   CHUNK = 64 * 1024;
constexpr uint32_t LOCAL_SIG = 0x04034b50, CENTRAL_SIG = 0x02014b50, END_SIG = 0x06054b50, DESCRIPTOR_SIG = 0x08074b50;
constexpr uint16_t FLAG_DESCRIPTOR = 0x0008, FLAG_UTF8 = 0x0800;

bool fail(std::string* why, const std::string& text)
{
    if (why) *why = text;
    return false;
}

struct FileCloser
{
    void operator()(FILE* f) const
    {
        if (f) std::fclose(f);
    }
};
using File = std::unique_ptr<FILE, FileCloser>;

void put16(std::string& out, uint32_t v)
{
    out += (char)(v & 0xff);
    out += (char)((v >> 8) & 0xff);
}
void put32(std::string& out, uint32_t v)
{
    put16(out, v & 0xffff);
    put16(out, v >> 16);
}
uint16_t get16(const uint8_t* p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}
uint32_t get32(const uint8_t* p)
{
    return (uint32_t)get16(p) | (uint32_t)get16(p + 2) << 16;
}

// The DOS date and time zip keeps, of a POSIX time (UTC).
void dos_time(time_t t, uint16_t* time_out, uint16_t* date_out)
{
    std::tm tm{};
    if (t <= 0 || !gmtime_r(&t, &tm) || tm.tm_year < 80) {
        *time_out = 0;
        *date_out = (1 << 5) | 1;   // 1980-01-01
        return;
    }
    *time_out = (uint16_t)(tm.tm_hour << 11 | tm.tm_min << 5 | tm.tm_sec / 2);
    *date_out = (uint16_t)((tm.tm_year - 80) << 9 | (tm.tm_mon + 1) << 5 | tm.tm_mday);
}

struct Found
{
    std::string rel;   // its name in the archive ("a/b.bin", a folder "a/")
    std::string path;  // on the disk
    time_t      mtime = 0;
    bool        folder = false;
};

// Everything under `dir`, folders before what they hold, by name.
bool walk(const std::string& dir, const std::string& rel, std::vector<Found>* out, std::string* why, int depth = 0)
{
    if (depth > 32) return fail(why, "the save's folders go too deep");
    DIR* d = ::opendir(dir.c_str());
    if (!d) return fail(why, "cannot read " + dir);
    std::vector<std::string> names;
    while (const dirent* e = ::readdir(d)) {
        const std::string n = e->d_name;
        if (n != "." && n != "..") names.push_back(n);
    }
    ::closedir(d);
    std::sort(names.begin(), names.end());
    for (const std::string& n : names) {
        const std::string path = (dir.empty() || dir.back() == '/' ? dir : dir + "/") + n;
        struct stat       st;
        if (::stat(path.c_str(), &st) != 0) return fail(why, "cannot read " + path);
        Found f;
        f.path = path;
        f.mtime = st.st_mtime;
        f.folder = S_ISDIR(st.st_mode);
        f.rel = rel + n + (f.folder ? "/" : "");
        if (f.rel.size() > MAX_NAME || out->size() >= MAX_ENTRIES) return fail(why, "the save holds too many files");
        out->push_back(f);
        if (f.folder && !walk(path, f.rel, out, why, depth + 1)) return false;
    }
    return true;
}

bool write_all(FILE* f, const std::string& data)
{
    return data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
}

struct Written
{
    std::string name;
    uint32_t    crc = 0;
    uint64_t    size = 0, packed = 0, offset = 0;
    uint16_t    method = 8, time = 0, date = 0;
    bool        folder = false;
};

std::string central(const Written& w)
{
    std::string h;
    put32(h, CENTRAL_SIG);
    put16(h, 20);   // made by: MS-DOS / any, zip 2.0
    put16(h, 20);
    put16(h, FLAG_UTF8 | (w.method == 8 ? FLAG_DESCRIPTOR : 0));
    put16(h, w.method);
    put16(h, w.time);
    put16(h, w.date);
    put32(h, w.crc);
    put32(h, (uint32_t)w.packed);
    put32(h, (uint32_t)w.size);
    put16(h, (uint32_t)w.name.size());
    put16(h, 0);   // extra
    put16(h, 0);   // comment
    put16(h, 0);   // disk
    put16(h, 0);   // internal attributes
    put32(h, w.folder ? 0x10 : 0);   // a folder, as DOS says
    put32(h, (uint32_t)w.offset);
    return h + w.name;
}

// One file deflated into `out` at `*at`, its descriptor after it.
bool add_file(FILE* out, const Found& f, Written* w, uint64_t* at, uint64_t* total, std::string* why)
{
    File in(std::fopen(f.path.c_str(), "rb"));
    if (!in) return fail(why, "cannot read " + f.path);
    std::string h;
    put32(h, LOCAL_SIG);
    put16(h, 20);
    put16(h, FLAG_UTF8 | FLAG_DESCRIPTOR);
    put16(h, 8);
    put16(h, w->time);
    put16(h, w->date);
    put32(h, 0);
    put32(h, 0);
    put32(h, 0);
    put16(h, (uint32_t)w->name.size());
    put16(h, 0);
    h += w->name;
    if (!write_all(out, h)) return fail(why, "cannot write the archive");
    z_stream z{};
    if (deflateInit2(&z, 1, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) return fail(why, "zlib cannot deflate");
    std::unique_ptr<z_stream, int (*)(z_stream*)> held(&z, deflateEnd);
    std::vector<uint8_t> buf(CHUNK), packed(CHUNK + 1024);
    uLong                crc = crc32(0, nullptr, 0);
    int                  flush = Z_NO_FLUSH;
    do {
        const size_t n = std::fread(buf.data(), 1, buf.size(), in.get());
        if (std::ferror(in.get())) return fail(why, "cannot read " + f.path);
        w->size += n;
        *total += n;
        if (*total > MAX_TOTAL) return fail(why, "the save is too large to send (4 GB at most)");
        crc = crc32(crc, buf.data(), (uInt)n);
        flush = std::feof(in.get()) ? Z_FINISH : Z_NO_FLUSH;
        z.next_in = buf.data();
        z.avail_in = (uInt)n;
        do {
            z.next_out = packed.data();
            z.avail_out = (uInt)packed.size();
            if (deflate(&z, flush) == Z_STREAM_ERROR) return fail(why, "zlib cannot deflate");
            const size_t got = packed.size() - z.avail_out;
            if (got && std::fwrite(packed.data(), 1, got, out) != got) return fail(why, "cannot write the archive");
            w->packed += got;
        } while (z.avail_out == 0);
    } while (flush != Z_FINISH);
    w->crc = (uint32_t)crc;
    std::string d;
    put32(d, DESCRIPTOR_SIG);
    put32(d, w->crc);
    put32(d, (uint32_t)w->packed);
    put32(d, (uint32_t)w->size);
    if (!write_all(out, d)) return fail(why, "cannot write the archive");
    *at += h.size() + w->packed + d.size();
    if (*at > MAX_TOTAL) return fail(why, "the save is too large to send (4 GB at most)");
    return true;
}

bool add_folder(FILE* out, Written* w, uint64_t* at, std::string* why)
{
    w->method = 0;
    std::string h;
    put32(h, LOCAL_SIG);
    put16(h, 20);
    put16(h, FLAG_UTF8);
    put16(h, 0);
    put16(h, w->time);
    put16(h, w->date);
    put32(h, 0);
    put32(h, 0);
    put32(h, 0);
    put16(h, (uint32_t)w->name.size());
    put16(h, 0);
    h += w->name;
    if (!write_all(out, h)) return fail(why, "cannot write the archive");
    *at += h.size();
    return true;
}

bool make_dirs(const std::string& path)
{
    for (size_t i = 1; i <= path.size(); i++) {
        if (i < path.size() && path[i] != '/') continue;
        const std::string part = path.substr(0, i);
        if (part.empty() || part.back() == ':') continue;   // "save:" itself
        if (::mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) {
            struct stat st;
            if (::stat(part.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return false;
        }
    }
    return true;
}
}   // namespace

bool zip_dir(const std::string& dir, const std::string& zip_path, const Progress& progress, std::string* why)
{
    std::vector<Found> found;
    if (!walk(dir, "", &found, why)) return false;
    File out(std::fopen(zip_path.c_str(), "wb"));
    if (!out) return fail(why, "cannot create " + zip_path);
    std::vector<Written> written;
    uint64_t             at = 0, total = 0;
    size_t               files = 0;
    for (const Found& f : found)
        files += !f.folder;
    size_t done = 0;
    for (const Found& f : found) {
        Written w;
        w.name = f.rel;
        w.offset = at;
        w.folder = f.folder;
        dos_time(f.mtime, &w.time, &w.date);
        if (f.folder) {
            if (!add_folder(out.get(), &w, &at, why)) return false;
        } else {
            if (progress) progress(done, files, f.rel);
            if (!add_file(out.get(), f, &w, &at, &total, why)) return false;
            done++;
        }
        written.push_back(w);
    }
    // An empty save: the mark, so the archive is no empty zip.
    if (found.empty()) {
        Written w;
        w.name = EMPTY_MARK;
        w.offset = at;
        w.method = 0;
        w.folder = false;
        dos_time(std::time(nullptr), &w.time, &w.date);
        std::string h;
        put32(h, LOCAL_SIG);
        put16(h, 20);
        put16(h, FLAG_UTF8);
        put16(h, 0);
        put16(h, w.time);
        put16(h, w.date);
        put32(h, 0);
        put32(h, 0);
        put32(h, 0);
        put16(h, (uint32_t)w.name.size());
        put16(h, 0);
        h += w.name;
        if (!write_all(out.get(), h)) return fail(why, "cannot write the archive");
        at += h.size();
        written.push_back(w);
    }
    std::string cd;
    for (const Written& w : written)
        cd += central(w);
    std::string end;
    put32(end, END_SIG);
    put16(end, 0);
    put16(end, 0);
    put16(end, (uint32_t)written.size());
    put16(end, (uint32_t)written.size());
    put32(end, (uint32_t)cd.size());
    put32(end, (uint32_t)at);
    put16(end, 0);
    if (at + cd.size() + end.size() > MAX_TOTAL) return fail(why, "the save is too large to send (4 GB at most)");
    if (!write_all(out.get(), cd) || !write_all(out.get(), end)) return fail(why, "cannot write the archive");
    if (std::fclose(out.release()) != 0) return fail(why, "cannot write the archive");
    if (progress) progress(files, files, "");
    return true;
}

bool safe_name(const std::string& name)
{
    if (name.empty() || name.size() > MAX_NAME || name[0] == '/') return false;
    size_t start = 0;
    const size_t end = name.back() == '/' ? name.size() - 1 : name.size();
    if (end == 0) return false;
    while (start <= end) {
        const size_t slash = name.find('/', start);
        const size_t stop = slash == std::string::npos || slash > end ? end : slash;
        const std::string part = name.substr(start, stop - start);
        if (part.empty() || part == "." || part == "..") return false;
        for (char c : part)
            if (c == '\\' || c == ':' || (unsigned char)c < 32 || c == 127) return false;
        if (stop >= end) break;
        start = stop + 1;
    }
    return true;
}

bool read_entries(const std::string& zip_path, std::vector<Entry>* out, std::string* why)
{
    out->clear();
    File f(std::fopen(zip_path.c_str(), "rb"));
    if (!f) return fail(why, "cannot read " + zip_path);
    if (std::fseek(f.get(), 0, SEEK_END) != 0) return fail(why, "cannot read the archive");
    const long size = std::ftell(f.get());
    if (size < 22) return fail(why, "the archive is no zip");
    // The end record, within the last 64 KB (its comment) and 22 bytes.
    const long tail = std::min<long>(size, 65535 + 22);
    std::vector<uint8_t> buf((size_t)tail);
    if (std::fseek(f.get(), size - tail, SEEK_SET) != 0 || std::fread(buf.data(), 1, buf.size(), f.get()) != buf.size())
        return fail(why, "cannot read the archive");
    long end = -1;
    for (long i = tail - 22; i >= 0; i--)
        if (get32(&buf[(size_t)i]) == END_SIG) {
            end = i;
            break;
        }
    if (end < 0) return fail(why, "the archive is no zip");
    const uint8_t* e = &buf[(size_t)end];
    const uint16_t count = get16(e + 10);
    const uint32_t cd_size = get32(e + 12), cd_offset = get32(e + 16);
    if (count == 0xffff || cd_offset == 0xffffffff) return fail(why, "the archive is a Zip64 one, too large");
    if ((uint64_t)cd_offset + cd_size > (uint64_t)size || count > MAX_ENTRIES) return fail(why, "the archive is damaged");
    std::vector<uint8_t> cd(cd_size);
    if (std::fseek(f.get(), (long)cd_offset, SEEK_SET) != 0 || (cd_size && std::fread(cd.data(), 1, cd.size(), f.get()) != cd.size()))
        return fail(why, "cannot read the archive");
    size_t   at = 0;
    uint64_t total = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (at + 46 > cd.size() || get32(&cd[at]) != CENTRAL_SIG) return fail(why, "the archive is damaged");
        const uint8_t* c = &cd[at];
        Entry          en;
        const uint16_t flags = get16(c + 8);
        en.method = get16(c + 10);
        en.crc = get32(c + 16);
        en.packed = get32(c + 20);
        en.size = get32(c + 24);
        const size_t name_len = get16(c + 28), extra = get16(c + 30), comment = get16(c + 32);
        en.offset = get32(c + 42);
        if (at + 46 + name_len + extra + comment > cd.size()) return fail(why, "the archive is damaged");
        en.name.assign((const char*)c + 46, name_len);
        at += 46 + name_len + extra + comment;
        if (flags & 0x0001) return fail(why, "the archive is encrypted");
        if (en.method != 0 && en.method != 8) return fail(why, "the archive uses a compression PlayShelf does not read");
        if (!safe_name(en.name)) return fail(why, "the archive holds a file it cannot be written as: " + en.name.substr(0, 64));
        en.folder = en.name.back() == '/';
        if (en.packed == 0xffffffff || en.size == 0xffffffff) return fail(why, "the archive is a Zip64 one, too large");
        if (en.offset + 30 > cd_offset || en.offset + 30 + en.packed > cd_offset) return fail(why, "the archive is damaged");
        total += en.size;
        if (total > MAX_TOTAL) return fail(why, "the archive is too large");
        out->push_back(std::move(en));
    }
    return true;
}

bool unzip_to(const std::string& zip_path, const std::string& dir, const std::function<bool(const std::string&)>& after_file,
              const Progress& progress, std::string* why)
{
    std::vector<Entry> entries;
    if (!read_entries(zip_path, &entries, why)) return false;
    File in(std::fopen(zip_path.c_str(), "rb"));
    if (!in) return fail(why, "cannot read " + zip_path);
    const std::string root = dir.empty() || dir.back() == '/' ? dir : dir + "/";
    size_t files = 0, done = 0;
    for (const Entry& en : entries)
        files += !en.folder;
    std::vector<uint8_t> buf(CHUNK), plain(CHUNK);
    for (const Entry& en : entries) {
        const std::string path = root + en.name;
        if (en.folder) {
            if (!make_dirs(path.substr(0, path.size() - 1))) return fail(why, "cannot make the folder " + en.name);
            continue;
        }
        if (progress) progress(done, files, en.name);
        done++;
        uint8_t lh[30];
        if (std::fseek(in.get(), (long)en.offset, SEEK_SET) != 0 || std::fread(lh, 1, 30, in.get()) != 30 ||
            get32(lh) != LOCAL_SIG)
            return fail(why, "the archive is damaged");
        const long data = (long)en.offset + 30 + get16(lh + 26) + get16(lh + 28);
        if (std::fseek(in.get(), data, SEEK_SET) != 0) return fail(why, "the archive is damaged");
        if (en.name == EMPTY_MARK) continue;
        const size_t slash = path.find_last_of('/');
        if (slash != std::string::npos && slash + 1 > root.size() && !make_dirs(path.substr(0, slash)))
            return fail(why, "cannot make the folder of " + en.name);
        File out(std::fopen(path.c_str(), "wb"));
        if (!out) return fail(why, "cannot write " + en.name);
        uLong    crc = crc32(0, nullptr, 0);
        uint64_t left = en.packed, written = 0;
        z_stream z{};
        std::unique_ptr<z_stream, int (*)(z_stream*)> held(nullptr, inflateEnd);
        if (en.method == 8) {
            if (inflateInit2(&z, -15) != Z_OK) return fail(why, "zlib cannot inflate");
            held.reset(&z);
        }
        bool ended = en.method == 0;
        while (left > 0 || (!ended && en.method == 8)) {
            const size_t want = (size_t)std::min<uint64_t>(left, buf.size());
            const size_t got = want ? std::fread(buf.data(), 1, want, in.get()) : 0;
            if (got != want) return fail(why, "the archive is cut short");
            left -= got;
            if (en.method == 0) {
                written += got;
                if (written > en.size || std::fwrite(buf.data(), 1, got, out.get()) != got)
                    return fail(why, "cannot write " + en.name);
                crc = crc32(crc, buf.data(), (uInt)got);
                continue;
            }
            z.next_in = buf.data();
            z.avail_in = (uInt)got;
            do {
                z.next_out = plain.data();
                z.avail_out = (uInt)plain.size();
                const int rc = inflate(&z, Z_NO_FLUSH);
                if (rc != Z_OK && rc != Z_STREAM_END && !(rc == Z_BUF_ERROR && z.avail_in == 0))
                    return fail(why, "the archive is damaged: " + en.name);
                const size_t n = plain.size() - z.avail_out;
                written += n;
                if (written > en.size) return fail(why, "the archive is damaged: " + en.name);
                if (n && std::fwrite(plain.data(), 1, n, out.get()) != n) return fail(why, "cannot write " + en.name);
                crc = crc32(crc, plain.data(), (uInt)n);
                if (rc == Z_STREAM_END) ended = true;
            } while (z.avail_out == 0 && !ended);
            if (!ended && left == 0) return fail(why, "the archive is cut short: " + en.name);
        }
        if (written != en.size || (uint32_t)crc != en.crc) return fail(why, "the archive is damaged: " + en.name);
        if (std::fclose(out.release()) != 0) return fail(why, "cannot write " + en.name);
        if (after_file && !after_file(en.name)) return fail(why, "stopped");
    }
    if (progress) progress(files, files, "");
    return true;
}

bool clear_dir(const std::string& dir, std::string* why)
{
    DIR* d = ::opendir(dir.c_str());
    if (!d) return fail(why, "cannot read " + dir);
    std::vector<std::string> names;
    while (const dirent* e = ::readdir(d)) {
        const std::string n = e->d_name;
        if (n != "." && n != "..") names.push_back(n);
    }
    ::closedir(d);
    for (const std::string& n : names) {
        const std::string path = (dir.back() == '/' ? dir : dir + "/") + n;
        struct stat       st;
        if (::stat(path.c_str(), &st) != 0) return fail(why, "cannot read " + path);
        if (S_ISDIR(st.st_mode)) {
            if (!clear_dir(path, why)) return false;
            if (::rmdir(path.c_str()) != 0) return fail(why, "cannot delete " + path);
        } else if (std::remove(path.c_str()) != 0) {
            return fail(why, "cannot delete " + path);
        }
    }
    return true;
}

}   // namespace save_archive
