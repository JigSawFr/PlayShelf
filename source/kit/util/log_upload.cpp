// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/log_upload.hpp"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <utility>

#include <aurore/extern/nlohmann/json.hpp>


#include <aurore/cadran/util/text.hpp>
#include <aurore/cadran/util/utf8.hpp>

#include "app_id.h"
#include "util/json_util.hpp"
#include "util/log_files.hpp"
#include "util/number.hpp"
#include "util/paths.hpp"

namespace log_upload
{

std::vector<Part> debug_files()
{
    std::vector<Part> out;
    std::string content;
    if (paths::read_file(paths::history_file(), content))
        out.push_back({ "history_recent.json", recent_history(content) });
    if (paths::read_file(paths::config_file(), content)) out.push_back({ "config.json", content });
    return out;
}

std::string recent_history(const std::string& text, size_t limit)
{
    nlohmann::json entries;
    json_util::parse_with(text, [&entries](const nlohmann::json& j) {
        const nlohmann::json* in = json_util::array_at(j, "entries");
        if (in) entries = *in;
        return in != nullptr;
    });
    if (!entries.is_array())
        return "(the change history could not be read: " + std::to_string(text.size()) + " bytes)\n";
    const size_t from = entries.size() > limit ? entries.size() - limit : 0;
    std::string out = "{\n \"entries_in_all\": " + std::to_string(entries.size()) + ",\n \"entries\": [";
    for (size_t i = from; i < entries.size(); i++) {
        nlohmann::json e = entries[i];
        if (e.is_object()) {
            e.erase("detail");
            const auto when = e.find("when");
            if (when != e.end() && when->is_string()) *when = when->get<std::string>().substr(0, 10);   // the date
        }
        out += (i > from ? ",\n  " : "\n  ") + e.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    }
    out += entries.size() > from ? "\n ]\n}\n" : "]\n}\n";
    return out;
}

std::vector<Part> crash_files(size_t limit)
{
    std::vector<Part> out;
    for (const std::string& name : log_files::list(paths::crashes_dir(), log_files::CRASH)) {
        if (out.size() >= limit) break;
        std::string content;
        if (paths::read_file(paths::crashes_dir() + "/" + name, content))
            out.push_back({ "logs/crashes/" + name, content });
    }
    return out;
}

std::string head_and_tail(const std::string& text, size_t max_bytes)
{
    if (text.size() <= max_bytes) return text;
    // The header: up to the first blank line, when it is short.
    size_t head = text.find("\n\n");
    head = head == std::string::npos || head + 2 > max_bytes / 4 ? 0 : head + 2;
    const std::string gap_note = "(... the lines in between left out ...)\n";
    const size_t room = max_bytes > head + gap_note.size() ? max_bytes - head - gap_note.size() : 0;
    size_t from = text.size() - room;
    // At a whole line: after the first line break at or past `from - 1`.
    const size_t nl = from == 0 ? std::string::npos : text.find('\n', from - 1);
    from = nl == std::string::npos ? text.size() : nl + 1;
    if (from < head) from = head;
    return text.substr(0, head) + gap_note + text.substr(from);
}

std::vector<Part> session_files(size_t limit)
{
    std::vector<Part> out;
    for (const std::string& name : log_files::list(paths::sessions_dir(), log_files::SESSION)) {
        if (out.size() >= limit) break;
        // Up to 1 MiB: only the part head_and_tail() keeps is read, its
        // header (in the first quarter) and one byte more than its end.
        std::string content;
        if (paths::read_ends(paths::sessions_dir() + "/" + name, SESSION_BYTES / 4, SESSION_BYTES + 1, content))
            out.push_back({ "logs/sessions/" + name, head_and_tail(content, SESSION_BYTES) });
    }
    return out;
}

std::vector<std::string> saved_reports(size_t limit)
{
    std::vector<std::string> names = log_files::list(paths::reports_dir(), log_files::REPORT);
    if (names.size() > limit) names.resize(limit);
    return names;
}

namespace
{
const std::string CUT_NOTICE = "\n\n(cut: too large to send whole)\n";

// The first `room` bytes of `text`, back to the start of a character (never
// half of a UTF-8 sequence), and the notice, cut too when `room` is smaller.
std::string cut_to(const std::string& text, size_t room)
{
    size_t keep = room > CUT_NOTICE.size() ? room - CUT_NOTICE.size() : 0;
    keep = utf8::boundary_before(text, keep);
    return text.substr(0, keep) + CUT_NOTICE.substr(0, room - keep);
}
}   // namespace

namespace
{
// The process of an Atmosphère crash report: its "Process Name:" line
// ("    Process Name:                    hbloader"), lower case.
std::string process_name(const std::string& text)
{
    const size_t at = text.find("Process Name:");
    if (at == std::string::npos) return "";
    const size_t end = text.find('\n', at);
    return text::lower_ascii(
        text::trim_space(text.substr(at + 13, end == std::string::npos ? std::string::npos : end - at - 13)));
}
}   // namespace

std::vector<Part> atmosphere_crashes(std::time_t now, int days, size_t limit)
{
    const std::string dir = paths::sd_root() + (paths::sd_root().back() == '/' ? "" : "/") + "atmosphere/crash_reports";
    // "01791569454_010000000000100d.log": the time, a program id.
    std::vector<std::pair<long long, std::string>> found;
    if (DIR* d = opendir(dir.c_str())) {
        while (const dirent* e = readdir(d)) {
            const std::string name = e->d_name;
            const size_t sep = name.find('_');
            if (sep == std::string::npos || sep == 0 || sep > 12 || !name.ends_with(".log") ||
                !std::all_of(name.begin(), name.begin() + sep, [](char c) { return c >= '0' && c <= '9'; }))
                continue;
            const long long t = *number::parse<long long>(std::string_view(name).substr(0, sep));   // digits, checked above
            if (t <= (long long)now && (long long)now - t <= (long long)days * 86400) found.push_back({ t, name });
        }
        closedir(d);
    }
    std::sort(found.begin(), found.end(),
              [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second > b.second; });
    std::vector<Part> out;
    for (const auto& f : found) {
        if (out.size() >= limit) break;
        // Only the start is sent (and says the process): one byte more is
        // read, to know it was cut.
        std::string content;
        if (!paths::read_ends(dir + "/" + f.second, ATMOSPHERE_CRASH_BYTES + 1, 0, content) ||
            process_name(content) != "hbloader")
            continue;
        if (content.size() > ATMOSPHERE_CRASH_BYTES) content = cut_to(content, ATMOSPHERE_CRASH_BYTES);
        out.push_back({ "atmosphere/crash_reports/" + f.second, content });
    }
    return out;
}

std::vector<Part> gist_files(const std::vector<Part>& parts, size_t max_bytes, bool* truncated)
{
    std::vector<Part> out;
    size_t total = 0;
    if (truncated) *truncated = false;
    for (size_t i = 0; i < parts.size(); i++) {
        // Its path under logs/, its folders joined with "_" (a gist file
        // name has no "/"): "logs/play_timer/block.json" is play_timer_block.json.
        std::string name = parts[i].name;
        if (text::starts_with(name, "logs/")) name.erase(0, 5);
        std::replace(name.begin(), name.end(), '/', '_');
        const std::string file =
            std::to_string(i + 1) + "-" + (name.contains('.') ? name : std::string("report.txt"));
        const std::string content = parts[i].content.empty() ? std::string("(empty)\n") : parts[i].content;
        if (total + content.size() <= max_bytes) {
            out.push_back({ file, content });
            total += content.size();
            continue;
        }
        if (truncated) *truncated = true;
        if (max_bytes - total > CUT_NOTICE.size()) out.push_back({ file, cut_to(content, max_bytes - total) });
        break;
    }
    return out;
}

size_t total_bytes(const std::vector<Part>& files)
{
    size_t n = 0;
    for (const Part& f : files) n += f.content.size();
    return n;
}

namespace
{
// "https://<host>/<id>" with the id made only of the given characters.
bool link_with_id(const std::string& text, const std::string& prefix, bool (*ok)(unsigned char))
{
    if (!text::starts_with(text, prefix)) return false;
    const std::string id = text.substr(prefix.size());
    return !id.empty() && id.size() <= 64 &&
           std::all_of(id.begin(), id.end(), [ok](char c) { return ok((unsigned char)c); });
}

}   // namespace

bool parse_gist(const std::string& body, std::string* url)
{
    return json_util::parse_with(body, [url](const nlohmann::json& j) {
        // "https://gist.github.com/<id>", or ".../<user>/<id>".
        const std::string link = json_util::string_at(j, "html_url");
        if (!link_with_id(link, "https://gist.github.com/", [](unsigned char c) {
                return std::isalnum(c) || c == '-' || c == '/';
            }) || link.find("//", 8) != std::string::npos || link.back() == '/')
            return false;
        *url = link;
        return true;
    });
}

std::string link_caption(const std::string& url)
{
    const std::string link = text::short_url(url);
    const size_t slash = link.rfind('/');
    if (slash == std::string::npos || slash + 1 == link.size()) return link;
    std::string out = link.substr(0, slash + 1) + "\n";
    for (size_t i = slash + 1; i < link.size(); i += 4) {
        if (i > slash + 1) out += ' ';
        out += link.substr(i, 4);
    }
    return out;
}

std::string issue_url(const std::string& repo_url, const std::string& paste_url, const IssueFields& fields)
{
    // Issue forms take each field's id as a query parameter.
    std::string url = repo_url + "/issues/new?template=1-bug.yml&report=" + text::url_encode(paste_url);
    auto add = [&url](const char* id, const std::string& value) {
        if (!value.empty()) url += std::string("&") + id + "=" + text::url_encode(value);
    };
    add("version", fields.version);
    add("firmware", fields.firmware);
    add("atmosphere", fields.atmosphere);
    add("launcher", fields.launcher);
    add("storage", fields.storage);
    return url;
}

std::string gist_body(const std::vector<Part>& files, const std::string& version, const std::string& what)
{
    nlohmann::json j;
    j["description"] = APP_NAME " " + version + " " + what;
    j["public"] = false;
    j["files"] = nlohmann::json::object();
    for (const Part& f : files) j["files"][f.name]["content"] = f.content;
    // A cut can never split a character (gist_files()); anything else invalid is replaced.
    return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

}   // namespace log_upload
