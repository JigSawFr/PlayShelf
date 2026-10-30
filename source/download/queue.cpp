// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "download/queue.hpp"

#include <algorithm>
#include <cstdio>
#include <random>
#include <utility>

#include <aurore/extern/nlohmann/json.hpp>

#include "util/paths.hpp"

namespace download
{
namespace
{
using json = nlohmann::json;

const char* const STATES[] = { "waiting", "running", "paused", "done", "failed" };

std::string str(const json& j, const char* key, size_t max = 2048)
{
    const auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return "";
    std::string s = it->get<std::string>();
    if (s.size() > max) s.resize(max);
    return s;
}

uint64_t u64(const json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_number_unsigned() ? it->get<uint64_t>() : 0;
}

bool flag(const json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_boolean() && it->get<bool>();
}

std::string hex(const uint8_t* p, size_t n)
{
    std::string out;
    char        b[3];
    for (size_t i = 0; i < n; i++) {
        std::snprintf(b, sizeof b, "%02x", p[i]);
        out += b;
    }
    return out;
}

bool unhex(const std::string& s, uint8_t* out, size_t n)
{
    if (s.size() != 2 * n) return false;
    for (size_t i = 0; i < n; i++) {
        unsigned v = 0;
        for (size_t k = 0; k < 2; k++) {
            const char c = s[2 * i + k];
            const int  d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0) return false;
            v = v << 4 | (unsigned)d;
        }
        out[i] = (uint8_t)v;
    }
    return true;
}

json journal_to_json(const install::Journal& jn)
{
    json j;
    json reg = json::array();
    for (const auto& id : jn.registered) reg.push_back(install::content_id_hex(id));
    j["registered"] = reg;
    if (jn.writing) j["writing"] = install::content_id_hex(*jn.writing);
    if (jn.at) {
        j["offset"] = jn.at->offset;
        j["nca_size"] = jn.at->nca_size;
        j["sha"] = hex(jn.at->sha.data(), jn.at->sha.size());
    }
    return j;
}

// Nothing when it is not a journal; a checkpoint that does not read is
// dropped (the content then starts again).
std::optional<install::Journal> journal_from_json(const json& j)
{
    if (!j.is_object()) return std::nullopt;
    install::Journal jn;
    const auto       reg = j.find("registered");
    if (reg != j.end() && reg->is_array())
        for (const json& r : *reg) {
            const auto id = r.is_string() ? install::content_id_from_hex(r.get<std::string>()) : std::nullopt;
            if (id && jn.registered.size() < 1024) jn.registered.push_back(*id);
        }
    if (const auto w = install::content_id_from_hex(str(j, "writing", 64))) {
        jn.writing = *w;
        install::ContentStream::Checkpoint at;
        at.offset = u64(j, "offset");
        at.nca_size = u64(j, "nca_size");
        if (unhex(str(j, "sha", 512), at.sha.data(), at.sha.size()) && at.offset && at.offset <= at.nca_size) jn.at = at;
    }
    return jn;
}
}   // namespace

std::string queue_to_json(const std::vector<Job>& jobs)
{
    json list = json::array();
    for (const Job& job : jobs) {
        json j;
        j["id"] = job.id;
        j["name"] = job.name;
        if (!job.game.empty()) j["game"] = job.game;
        if (!job.label.empty()) j["label"] = job.label;
        if (job.game_id) j["game_id"] = job.game_id;
        if (!job.icon_url.empty()) j["icon"] = job.icon_url;
        j["url"] = job.url;
        j["server"] = job.server_id;
        j["title_id"] = job.title_id;
        if (job.version) j["version"] = *job.version;
        j["size"] = job.size;
        j["state"] = STATES[std::to_underlying(job.state)];
        if (!job.error.empty()) j["error"] = job.error;
        if (job.unsigned_content) j["unsigned"] = true;
        if (job.accept_unsigned) j["accept_unsigned"] = true;
        j["done"] = job.done;
        j["total"] = job.total;
        if (job.journal) j["journal"] = journal_to_json(*job.journal);
        list.push_back(j);
    }
    json root;
    root["schema"] = 1;
    root["jobs"] = list;
    return root.dump(2) + "\n";
}

std::vector<Job> queue_from_json(const std::string& text)
{
    std::vector<Job> out;
    const json       root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return out;
    const auto list = root.find("jobs");
    if (list == root.end() || !list->is_array()) return out;
    for (const json& j : *list) {
        if (!j.is_object() || out.size() >= MAX_JOBS) continue;
        Job job;
        job.id = str(j, "id", 64);
        job.name = str(j, "name", 256);
        job.game = str(j, "game", 256);
        job.label = str(j, "label", 256);
        job.game_id = u64(j, "game_id");
        job.icon_url = str(j, "icon");
        if (!job.icon_url.starts_with("https://")) job.icon_url.clear();
        job.url = str(j, "url");
        job.server_id = str(j, "server", 64);
        job.title_id = u64(j, "title_id");
        if (const auto v = j.find("version"); v != j.end() && v->is_number_unsigned() && v->get<uint64_t>() <= UINT32_MAX)
            job.version = (uint32_t)v->get<uint64_t>();
        job.size = u64(j, "size");
        const std::string state = str(j, "state", 16);
        for (int i = 0; i < 5; i++)
            if (state == STATES[i]) job.state = (State)i;
        // Running when PlayShelf stopped: cut short, to be taken up again.
        if (job.state == State::Running) job.state = State::Paused;
        job.error = str(j, "error", 512);
        job.unsigned_content = flag(j, "unsigned");
        job.accept_unsigned = flag(j, "accept_unsigned");
        job.done = u64(j, "done");
        job.total = u64(j, "total");
        const auto jn = j.find("journal");
        if (jn != j.end()) job.journal = journal_from_json(*jn);
        // Only an https address is ever fetched (a file edited by hand).
        if (job.id.empty() || !job.url.starts_with("https://")) continue;
        out.push_back(std::move(job));
    }
    return out;
}

std::string queue_file() { return paths::data_dir() + "/queue.json"; }

std::vector<Job> load_queue()
{
    std::string text;
    if (!paths::read_file(queue_file(), text)) return {};
    return queue_from_json(text);
}

bool save_queue(const std::vector<Job>& jobs, std::string* why)
{
    if (!paths::ensure_dir(paths::data_dir())) {
        if (why) *why = "cannot create " + paths::data_dir();
        return false;
    }
    return paths::atomic_write(queue_file(), queue_to_json(jobs), why);
}

std::string new_job_id()
{
    static const char  HEX[] = "0123456789abcdef";
    std::random_device rd;
    std::string        id;
    for (int i = 0; i < 16; i++)
        id += HEX[rd() & 15];
    return id;
}

long next_job(const std::vector<Job>& jobs)
{
    for (size_t i = 0; i < jobs.size(); i++)
        if (jobs[i].state == State::Waiting) return (long)i;
    return -1;
}

size_t pending(const std::vector<Job>& jobs)
{
    return (size_t)std::count_if(jobs.begin(), jobs.end(), [](const Job& j) {
        return j.state == State::Waiting || j.state == State::Running || j.state == State::Paused;
    });
}

std::vector<size_t> ahead_jobs(const std::vector<Job>& jobs, const std::string& running_id, int count)
{
    std::vector<size_t> out;
    for (size_t i = 0; i < jobs.size() && (int)out.size() < count; i++)
        if (jobs[i].state == State::Waiting && jobs[i].id != running_id) out.push_back(i);
    return out;
}

bool room_ahead(uint64_t free, uint64_t left, uint64_t size, uint64_t reserve)
{
    const uint64_t spare = 1ull << 30;
    const uint64_t need = left + size + reserve + spare;
    return need >= left && free >= need;   // no wrap around
}

bool queued(const std::vector<Job>& jobs, const std::string& url)
{
    return std::any_of(jobs.begin(), jobs.end(), [&](const Job& j) {
        return j.url == url && j.state != State::Done && j.state != State::Failed;
    });
}

std::vector<install::ContentId> orphans(const std::vector<install::ContentId>& present, const std::vector<Job>& jobs)
{
    std::vector<install::ContentId> out;
    for (const auto& id : present) {
        const bool kept = std::any_of(jobs.begin(), jobs.end(), [&](const Job& j) {
            return j.state != State::Done && j.journal && j.journal->writing == id;
        });
        if (!kept) out.push_back(id);
    }
    return out;
}

std::vector<Group> groups(const std::vector<Job>& jobs)
{
    std::vector<Group> out;
    for (size_t i = 0; i < jobs.size(); i++) {
        const Job& j = jobs[i];
        Group*     g = nullptr;
        if (j.game_id)
            for (Group& o : out) {
                const Job& first = jobs[o.jobs.front()];
                if (first.game_id == j.game_id && first.server_id == j.server_id) g = &o;
            }
        if (!g) {
            out.emplace_back();
            g = &out.back();
            g->game = j.game.empty() ? j.name : j.game;
        }
        if (g->icon_url.empty()) g->icon_url = j.icon_url;
        g->jobs.push_back(i);
    }
    return out;
}

std::vector<Segment> timeline(const Group& g, const std::vector<Job>& jobs, const std::string& running_id,
                              uint64_t running_done, uint64_t running_total)
{
    std::vector<Segment> out;
    double               sum = 0;
    for (size_t i : g.jobs) {
        const Job& j = jobs[i];
        Segment    s;
        // A package of unknown size counts as the smallest known one's.
        s.weight = (double)(j.size ? j.size : j.total);
        s.state = j.state;
        if (j.state == State::Done) s.filled = 1;
        else if (j.id == running_id && running_total) s.filled = std::min(1.0, (double)running_done / (double)running_total);
        else if (j.total) s.filled = std::min(1.0, (double)j.done / (double)j.total);
        out.push_back(s);
    }
    double smallest = 0;
    for (const Segment& s : out)
        if (s.weight > 0 && (smallest == 0 || s.weight < smallest)) smallest = s.weight;
    for (Segment& s : out) {
        if (s.weight <= 0) s.weight = smallest > 0 ? smallest : 1;
        sum += s.weight;
    }
    for (Segment& s : out) s.weight /= sum;
    return out;
}

State group_state(const Group& g, const std::vector<Job>& jobs)
{
    auto any = [&](State st) {
        return std::any_of(g.jobs.begin(), g.jobs.end(), [&](size_t i) { return jobs[i].state == st; });
    };
    if (any(State::Running)) return State::Running;
    if (any(State::Failed)) return State::Failed;
    if (any(State::Waiting)) return State::Waiting;
    if (any(State::Paused)) return State::Paused;
    return State::Done;
}

void Rate::add(uint64_t bytes, double seconds)
{
    // A count that went back (another job, a restart of the content): anew.
    if (!samples.empty() && (bytes < samples.back().second || seconds < samples.back().first)) samples.clear();
    samples.emplace_back(seconds, bytes);
    while (samples.size() > 2 && seconds - samples[1].first >= WINDOW) samples.pop_front();
}

double Rate::bytes_per_second() const
{
    if (samples.size() < 2) return 0;
    const double span = samples.back().first - samples.front().first;
    if (span < 1.0) return 0;
    return (double)(samples.back().second - samples.front().second) / span;
}

std::optional<double> Rate::eta(uint64_t left) const
{
    const double speed = bytes_per_second();
    if (speed <= 0) return std::nullopt;
    return (double)left / speed;
}

}   // namespace download
