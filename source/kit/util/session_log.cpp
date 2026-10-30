// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/session_log.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <unistd.h>

#include <aurore/cadran/util/utf8.hpp>

#include "util/date.hpp"
#include "util/log_files.hpp"
#include "util/paths.hpp"

namespace session_log
{

namespace
{
// The lines in memory: slot count % RING_LINES is the next one. Written under
// g_mutex; read without it by recent(), which a crash handler calls on a
// thread that may hold it (a line half written then reads cut short).
char                  g_ring[RING_LINES][LINE_BYTES];
std::atomic<uint64_t> g_count{ 0 };

std::mutex  g_mutex;
// This run's file. Opened for each write, not kept open: the console's SD
// card refuses to read or delete a file open for writing (a report sent
// online with it, Developer › Delete the log files).
char        g_path[640];
size_t      g_bytes = 0;          // written to it, or waiting in g_pending
bool        g_full = false;       // past MAX_BYTES: nothing more is written
std::atomic<bool> g_ended{ false };
std::string g_name;
std::chrono::steady_clock::time_point g_started;
bool        g_has_start = false;

// The lines not written yet, under g_mutex. end_crash() writes them without
// the lock (a crash on the thread that holds it): the length is stored once
// the bytes are in.
char                  g_pending[PENDING_BYTES];
std::atomic<size_t>   g_pending_len{ 0 };
std::atomic<long long> g_pending_since{ 0 };   // steady ms of the first of them

long long steady_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Appends to the file at `path`, created 0644 (as util/pt_log.cpp) when it
// went (Delete the log files skips it, but a computer may not). No allocation.
bool append(const char* path, const char* data, size_t size) noexcept
{
    const int fd = ::open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) return false;
    const bool ok = write_all(fd, data, size);
    return ::close(fd) == 0 && ok;
}

// What waits, into the file at once, under g_mutex. Lines the SD card
// refused are lost, as they were one by one.
void write_pending()
{
    const size_t n = g_pending_len.load(std::memory_order_acquire);
    if (n == 0) return;
    if (!append(g_path, g_pending, n)) g_bytes -= n;
    g_pending_len.store(0, std::memory_order_release);
}

// Into the file, under g_mutex: with the lines that wait, written together
// (write_pending()).
void to_file(const std::string& text)
{
    if (!g_path[0] || g_full || g_ended) return;
    if (g_bytes + text.size() > MAX_BYTES) {
        write_pending();
        const char note[] = "(this file stops here: over 1 MiB; the next lines were not written)\n";
        append(g_path, note, sizeof(note) - 1);
        g_full = true;
        return;
    }
    g_bytes += text.size();
    size_t n = g_pending_len.load(std::memory_order_relaxed);
    if (n + text.size() > PENDING_BYTES) {
        write_pending();
        n = 0;
    }
    if (text.size() > PENDING_BYTES) {   // a message longer than the buffer: on its own
        if (!append(g_path, text.data(), text.size())) g_bytes -= text.size();
        return;
    }
    if (n == 0) g_pending_since.store(steady_ms(), std::memory_order_relaxed);
    std::memcpy(g_pending + n, text.data(), text.size());
    g_pending_len.store(n + text.size(), std::memory_order_release);
}

bool due_now()
{
    return g_pending_len.load(std::memory_order_acquire) > 0 &&
           steady_ms() - g_pending_since.load(std::memory_order_relaxed) >= FLUSH_MS;
}

// Into memory, under g_mutex: one line, cut to LINE_BYTES (on a whole UTF-8
// character), its own line breaks as spaces.
void to_ring(const std::string& line)
{
    char* slot = g_ring[g_count.load(std::memory_order_relaxed) % RING_LINES];
    size_t n = line.size();
    if (n > 0 && line[n - 1] == '\n') n--;
    if (n > LINE_BYTES - 2) n = utf8::boundary_before(line, LINE_BYTES - 2);
    for (size_t i = 0; i < n; i++) slot[i] = line[i] == '\n' || line[i] == '\r' ? ' ' : line[i];
    slot[n] = '\n';
    slot[n + 1] = '\0';
    g_count.fetch_add(1, std::memory_order_release);
}
}   // namespace

bool write_all(int fd, const char* data, size_t size)
{
    while (size > 0) {
        const ssize_t n = ::write(fd, data, size);
        if (n <= 0) return false;
        data += n;
        size -= (size_t)n;
    }
    return true;
}

std::string format(std::time_t t, int ms, char level, const std::string& text)
{
    char head[24];
    std::snprintf(head, sizeof(head), "%s.%03d %c ", date::local_time(t, "%H:%M:%S").c_str(), ms < 0 || ms > 999 ? 0 : ms,
                  level);
    std::string out = head;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\r') continue;
        out += text[i];
        if (text[i] == '\n' && i + 1 < text.size()) out += "    ";
    }
    while (!out.empty() && out.back() == '\n') out.pop_back();
    return out + "\n";
}

std::string start(const std::string& dir, std::time_t now, const std::string& header)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_has_start) {
        g_started = std::chrono::steady_clock::now();
        g_has_start = true;
    }
    if (g_path[0] || g_ended || !paths::ensure_dir(dir)) return g_name;
    const std::string st = log_files::stamp(now);
    for (unsigned i = 0; i < 100; i++) {
        const std::string name = log_files::name(log_files::SESSION, st, i);
        const std::string path = dir + "/" + name;
        if (path.size() >= sizeof(g_path)) break;
        // O_EXCL: a name another file has (a run in the same second) is not reused.
        const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd >= 0) {
            ::close(fd);
            std::snprintf(g_path, sizeof(g_path), "%s", path.c_str());
            g_name = name;
            break;
        }
        if (errno != EEXIST) break;
    }
    if (!g_path[0]) return "";
    to_file(header);
    // What was logged before the file existed.
    const uint64_t count = g_count.load(std::memory_order_acquire);
    const uint64_t first = count > RING_LINES ? count - RING_LINES : 0;
    if (first > 0) to_file("(" + std::to_string(first) + " earlier line(s) not kept)\n");
    for (uint64_t i = first; i < count; i++) to_file(g_ring[i % RING_LINES]);
    write_pending();
    return g_name;
}

void add(std::time_t t, int ms, char level, const std::string& text)
{
    const std::string line = format(t, ms, level, text);
    std::lock_guard<std::mutex> lock(g_mutex);
    to_ring(line);
    to_file(line);
    // A warning or an error may come just before a crash: on the card now.
    if (g_path[0] && !g_ended && (level == 'E' || level == 'W' || due_now())) write_pending();
}

bool due()
{
    return due_now();
}

void flush()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_path[0] && !g_ended) write_pending();
}

void end(const std::string& reason)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_ended || !g_path[0]) {
        g_ended = true;
        return;
    }
    write_pending();   // before the end line: nothing logged is lost
    if (g_ended.exchange(true)) return;
    const std::string line = std::string(END) + reason + " ===\n";
    append(g_path, line.data(), line.size());
}

void end_crash(const char* crash_name) noexcept
{
    if (g_ended.exchange(true) || !g_path[0]) return;
    // The lines still waiting, then the end line (no lock: see g_pending).
    const size_t waiting = g_pending_len.exchange(0, std::memory_order_acq_rel);
    if (waiting > 0) append(g_path, g_pending, std::min(waiting, sizeof(g_pending)));
    char line[160];
    const int n = std::snprintf(line, sizeof(line), "%scrash, see logs/crashes/%s ===\n", END,
                                crash_name && *crash_name ? crash_name : "(no crash file)");
    if (n > 0) append(g_path, line, std::min((size_t)n, sizeof(line) - 1));
}

size_t recent(char* out, size_t size) noexcept
{
    if (!out || size == 0) return 0;
    out[0] = '\0';
    const uint64_t count = g_count.load(std::memory_order_acquire);
    const uint64_t kept = count < RING_LINES ? count : RING_LINES;
    // As many of the last lines as fit.
    uint64_t first = count;
    size_t need = 1;
    while (first > count - kept) {
        const size_t n = strnlen(g_ring[(first - 1) % RING_LINES], LINE_BYTES);
        if (need + n > size) break;
        need += n;
        first--;
    }
    size_t len = 0;
    for (uint64_t i = first; i < count; i++) {
        const char* line = g_ring[i % RING_LINES];
        const size_t n = strnlen(line, LINE_BYTES);
        if (len + n + 1 > size) break;
        std::memcpy(out + len, line, n);
        len += n;
    }
    out[len] = '\0';
    return len;
}

std::string recent()
{
    std::string out(RING_LINES * LINE_BYTES + 1, '\0');
    out.resize(recent(&out[0], out.size()));
    return out;
}

std::string current_name()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_name;
}

long long uptime_s()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_has_start) return 0;
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - g_started).count();
}

std::string end_reason(const std::string& text)
{
    size_t end = text.size();
    while (end > 0 && (text[end - 1] == '\n' || text[end - 1] == '\r')) end--;
    if (end == 0) return "";
    const size_t from = text.rfind('\n', end - 1);
    const size_t start = from == std::string::npos ? 0 : from + 1;
    const size_t mark = std::strlen(END);
    if (text.compare(start, mark, END) != 0) return "";
    std::string reason = text.substr(start + mark, end - start - mark);
    if (reason.ends_with(" ===")) reason.resize(reason.size() - 4);
    return reason.empty() ? "?" : reason;
}

bool finished(const std::string& text)
{
    return !end_reason(text).empty();
}

std::string last_lines(const std::string& text, size_t n)
{
    if (n == 0) return "";
    size_t end = text.size();
    if (end > 0 && text[end - 1] == '\n') end--;
    size_t from = end;
    for (size_t found = 0; from > 0; from--) {
        if (text[from - 1] == '\n' && ++found == n) break;
    }
    return text.substr(from);
}

bool tail_of(const std::string& path, std::string* out)
{
    std::string text;
    if (!paths::read_ends(path, 0, TAIL_BYTES + 1, text)) return false;
    // Cut: from its first whole line.
    if (text.size() > TAIL_BYTES) {
        const size_t nl = text.find('\n');
        text.erase(0, nl == std::string::npos ? text.size() : nl + 1);
    }
    *out = std::move(text);
    return true;
}

bool unfinished(const std::string& dir, const std::string& current, Unfinished* out)
{
    for (const std::string& name : log_files::list(dir, log_files::SESSION)) {
        if (name == current) continue;
        // The newest other one only: an older one without an end line was
        // reported at the start after it. Its end only (a file can reach
        // 1 MiB): the end line, and the last lines for the crash file.
        std::string text;
        if (!tail_of(dir + "/" + name, &text) || finished(text)) return false;
        if (out) *out = { name, text };
        return true;
    }
    return false;
}

void reset()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_path[0] = '\0';
    g_bytes = 0;
    g_full = false;
    g_ended = false;
    g_name.clear();
    g_has_start = false;
    g_count = 0;
    g_pending_len = 0;
}

}   // namespace session_log
