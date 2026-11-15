// link/link_state.cpp: the `state` document built from the downloads' queue
// (counts, the running job's whole percents, the last game finished, the SD
// card's room), what it holds when nothing is known, a name that needs
// escaping, and the orders it takes (the pause button, nothing else).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstring>

#include "check.h"
#include "link/link_state.hpp"

using namespace link_state;
using download::Job;
using download::State;

namespace
{
Job job(State state, const char* name, const char* game = "")
{
    Job j;
    j.id = name;
    j.name = name;
    j.game = game;
    j.state = state;
    return j;
}

bool has(const std::string& text, const char* part)
{
    return text.find(part) != std::string::npos;
}

void test_counts_and_progress()
{
    const std::vector<Job> jobs = {job(State::Done, "Old Quest · Update", "Old Quest"),
                                   job(State::Running, "Moon Racer"), job(State::Waiting, "Tiny Farm"),
                                   job(State::Paused, "Sky Fort"), job(State::Failed, "Dust Maze"),
                                   job(State::Done, "Pixel Cats")};
    const Summary s = summarize(jobs, 250, 1000, true, 5ull << 30, "1.4.0");
    CHECK(s.pending == 3 && s.running == 1 && s.paused == 1 && s.failed == 1);
    CHECK(s.progress == 25);
    CHECK(s.last_installed == "Pixel Cats");   // the last one finished, by the game's name else the job's
    CHECK(s.free_known && s.free_bytes == (5ull << 30) && s.version == "1.4.0");

    const std::string text = state_json(s);
    CHECK(has(text, "\"schema\":1") && has(text, "\"version\":\"1.4.0\""));
    CHECK(has(text, "\"downloads\":{\"pending\":3,\"running\":1,\"paused\":1,\"failed\":1,\"progress\":25}"));
    CHECK(has(text, "\"last_installed\":\"Pixel Cats\"") && has(text, "\"storage\":{\"free\":5368709120}"));
    CHECK(text == state_json(s));   // the same summary, the same text
}

void test_game_name_over_job_name()
{
    const Summary s = summarize({job(State::Done, "Old Quest · Update v3", "Old Quest")}, 0, 0, false, 0, "1.0.0");
    CHECK(s.last_installed == "Old Quest");
}

void test_nothing_known()
{
    const Summary s = summarize({}, 0, 0, false, 0, "1.0.0");
    CHECK(s.pending == 0 && s.progress == -1 && s.last_installed.empty() && !s.free_known);
    const std::string text = state_json(s);
    CHECK(has(text, "\"progress\":null") && has(text, "\"last_installed\":null") && has(text, "\"storage\":{\"free\":null}"));
    CHECK(has(text, "\"pending\":0"));
}

void test_progress_bounds()
{
    // Whole percents; a running job whose size is unknown has none; more done than total is full.
    const std::vector<Job> running = {job(State::Running, "A")};
    CHECK(summarize(running, 0, 0, false, 0, "v").progress == -1);
    CHECK(summarize(running, 10, 0, false, 0, "v").progress == -1);
    CHECK(summarize(running, 999, 1000, false, 0, "v").progress == 99);
    CHECK(summarize(running, 5000, 1000, false, 0, "v").progress == 100);
    // Nothing runs: the figures of a job that just ended say nothing.
    CHECK(summarize({job(State::Paused, "A")}, 500, 1000, false, 0, "v").progress == -1);
}

void test_escaping_and_size()
{
    const Summary s = summarize({job(State::Done, "He said \"hi\"\n\\ \xC3\xA9")}, 0, 0, false, 0, "1.0.0");
    const std::string text = state_json(s);
    CHECK(has(text, "He said \\\"hi\\\"\\n\\\\ ") && text.find('\n') == std::string::npos);
    // A name too long for the document: no document, rather than a cut one.
    const Summary big = summarize({job(State::Done, std::string(2000, 'x').c_str())}, 0, 0, false, 0, "1.0.0");
    CHECK(state_json(big).empty());
}

void test_orders()
{
    CHECK(parse_order("pause_downloads/set", "PRESS") == Order::PauseDownloads);
    CHECK(parse_order("pause_downloads/set", "") == Order::None);            // a retained order cleared
    CHECK(parse_order("pause_downloads/set", "press") == Order::None);
    CHECK(parse_order("pause_downloads/set", "PRESS ") == Order::None);
    CHECK(parse_order("other/set", "PRESS") == Order::None);
    CHECK(parse_order("pause_downloads", "PRESS") == Order::None);
    CHECK(parse_order("", "") == Order::None);
}
}   // namespace

int main()
{
    test_counts_and_progress();
    test_game_name_over_job_name();
    test_nothing_known();
    test_progress_bounds();
    test_escaping_and_size();
    test_orders();
    return CHECK_DONE("link_state: counts, progress, last game, storage, escaping and orders assertions passed");
}
