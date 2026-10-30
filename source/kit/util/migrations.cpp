// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/migrations.hpp"

#include <chrono>

#include <aurore/extern/nlohmann/json.hpp>

#include "app_id.h"
#include "util/json_util.hpp"
#include "util/paths.hpp"
#include "util/update.hpp"

namespace migrations
{

using nlohmann::json;

namespace
{
enum class Found
{
    Missing,
    Unreadable,
    Read,
};

Found read_state(const std::string& file, State* out)
{
    *out = State{};
    std::string text;
    if (!paths::read_file(file, text)) return paths::exists_or_pending(file) ? Found::Unreadable : Found::Missing;
    State s;
    const bool ok = json_util::parse_with(text, [&s](const json& j) {
        if (!j.is_object()) return false;
        const auto level = j.find("level");
        if (level == j.end() || !level->is_number_integer() || level->get<long long>() < 0 ||
            level->get<long long>() > 0x7FFF)
            return false;
        s.level   = (int)level->get<long long>();
        s.version = json_util::string_at(j, "version");
        return true;
    });
    if (!ok) return Found::Unreadable;
    *out = s;
    return Found::Read;
}

std::string build_name(const std::string& version)
{
    return APP_NAME " " + (version.empty() ? std::string("?") : version);
}

// "steps 1 to 3", "step 3": the levels of `steps` (in ascending order).
std::string span(const std::vector<Step>& steps)
{
    if (steps.empty()) return "no step";
    if (steps.size() == 1) return "step " + std::to_string(steps.front().level);
    return "steps " + std::to_string(steps.front().level) + " to " + std::to_string(steps.back().level);
}

// "steps 1, 3": each of `steps`, for a choice that may skip some.
std::string names(const std::vector<Step>& steps)
{
    if (steps.empty()) return "no step";
    std::string out = steps.size() == 1 ? "step " : "steps ";
    for (size_t i = 0; i < steps.size(); i++) out += (i ? ", " : "") + std::to_string(steps[i].level);
    return out;
}

bool write_logged(const std::string& file, const State& state, const Log& log)
{
    std::string err;
    const size_t slash = file.find_last_of('/');
    if (slash != std::string::npos) paths::ensure_dir(file.substr(0, slash));
    const json j = { { "schema", SCHEMA }, { "level", state.level }, { "version", state.version } };
    const bool ok = paths::atomic_write(file, j.dump() + "\n", &err);
    if (log) {
        if (ok)
            log(false, "migrations: level " + std::to_string(state.level) + " and " + build_name(state.version) +
                           " written to " + file);
        else
            log(true, "migrations: could not write " + file + " (" + err +
                          "): the steps above the level recorded run again at the next start");
    }
    return ok;
}
}   // namespace

bool read(const std::string& file, State* out)
{
    return read_state(file, out) == Found::Read;
}

bool write(const std::string& file, const State& state)
{
    return write_logged(file, state, nullptr);
}

namespace
{
int s_latest = None;
}

void set_latest(int level) { s_latest = level; }
int  latest() { return s_latest; }

size_t run(const std::string& file, const std::vector<Step>& steps, const std::string& version, const Log& log,
           bool all)
{
    State was;
    const Found found = read_state(file, &was);
    if (log) {
        if (found == Found::Missing)
            log(false, "migrations: no " + file + " (a first start, or a build from before it): " + span(steps) +
                           " to run");
        else if (found == Found::Unreadable)
            log(true, "migrations: " + file + " unreadable (damaged): " + span(steps) + " to run again");
        else
            log(false, "migrations: level " + std::to_string(was.level) + " recorded, last started by " +
                           build_name(was.version));
    }
    State s = was;
    if (found == Found::Read && log && !was.version.empty() && was.version != version) {
        const int order = update::compare_versions(was.version, version);
        log(false, "migrations: " + build_name(was.version) + " started last, " +
                       (order < 0 ? "an earlier build (an update since)" : order > 0 ? "a later build" : "another build") +
                       "; this is " + build_name(version));
    }
    const int known = latest();
    if (s.level > known) {
        // A later build's layout: this one writes its own, the steps above
        // it run again when the later build is back.
        if (log)
            log(false, "migrations: level " + std::to_string(s.level) + " is a later build's (this one knows up to " +
                           std::to_string(known) + "): back to " + std::to_string(known) +
                           ", so that build runs the steps above it again");
        s.level = known;
    }
    const int kept = s.level;
    if (all) {
        if (log) log(false, "migrations: " + names(steps) + " run again, whatever level is recorded");
        s.level = None;
    }
    size_t ran = 0;
    for (const Step& st : steps) {
        if (st.level <= s.level) continue;
        const auto t0 = std::chrono::steady_clock::now();
        const size_t moved = st.run ? st.run() : 0;
        const long long ms =
            (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        if (log)
            log(false, "migration " + std::to_string(st.level) + " (" + st.name + "): " + std::to_string(moved) +
                           " entries moved in " + std::to_string(ms) + " ms");
        s.level = st.level;
        ran++;
    }
    if (all && kept > s.level) s.level = kept;
    if (!ran && log && !steps.empty()) log(false, "migrations: " + span(steps) + " already done, nothing run");
    s.version = version;
    if (s.level != was.level || s.version != was.version || found != Found::Read) write_logged(file, s, log);
    return ran;
}

bool earlier_build_next(const std::string& file, const Log& log)
{
    State s;
    const Found found = read_state(file, &s);
    if (found == Found::Missing) {
        if (log) log(false, "migrations: no " + file + ": every step runs at the next start anyway");
        return true;
    }
    if (found == Found::Read && s.level == None) {
        if (log) log(false, "migrations: level already 0: every step runs again at the next start");
        return true;
    }
    if (log)
        log(false, "migrations: an earlier build (or one whose layout is not known) is next: level " +
                       (found == Found::Read ? std::to_string(s.level) : std::string("unreadable")) +
                       " set to 0, every step runs again at the next start of this build or a later one");
    s.level = None;
    return write_logged(file, s, log);
}

bool is_earlier(const std::string& version, const std::string& running)
{
    return version.empty() || update::compare_versions(version, running) <= 0;
}

}   // namespace migrations
