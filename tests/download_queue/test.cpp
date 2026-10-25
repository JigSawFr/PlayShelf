// download/queue.cpp: the queue's file written and read back (every field,
// a journal and its checkpoint), a running job back as paused, what a
// hand-edited file cannot hold dropped; the next job, what is pending, a
// package queued twice; the placeholders no job keeps; the speed and the
// time left.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "download/queue.hpp"
#include "fs.hpp"
#include "temp_dir.hpp"

using namespace download;

namespace
{
install::ContentId cid(uint8_t b)
{
    install::ContentId id{};
    id.fill(b);
    return id;
}

Job job(const char* id, State state, const char* url = "https://shelf.example/files/a.nsp")
{
    Job j;
    j.id = id;
    j.name = std::string("Game ") + id;
    j.url = url;
    j.state = state;
    return j;
}

void test_file()
{
    TempDir tmp("download_queue", TempDir::Enter);
    Job     a = job("00000000000000a1", State::Running);
    a.server_id = "0123456789abcdef";
    a.title_id = 0x0100000000010000ull;
    a.size = 123456789;
    a.done = 1000;
    a.total = 5000;
    install::Journal jn;
    jn.registered = {cid(1), cid(2)};
    jn.writing = cid(3);
    install::ContentStream::Checkpoint at;
    at.offset = 0x8000;
    at.nca_size = 0x10000;
    for (size_t i = 0; i < at.sha.size(); i++) at.sha[i] = (uint8_t)i;
    jn.at = at;
    a.journal = jn;
    Job b = job("00000000000000b2", State::Failed, "https://shelf.example/files/b.nsz");
    b.error = "not signed by Nintendo";
    b.unsigned_content = true;
    b.accept_unsigned = true;
    std::string why;
    CHECK(save_queue({a, b}, &why));
    const std::vector<Job> back = load_queue();
    if (!CHECK(back.size() == 2)) return;
    CHECK(back[0].state == State::Paused);   // cut short by a restart
    CHECK(back[0].name == a.name && back[0].url == a.url && back[0].server_id == a.server_id);
    CHECK(back[0].title_id == a.title_id && back[0].size == a.size && back[0].done == 1000 && back[0].total == 5000);
    CHECK(back[0].journal && *back[0].journal == jn);
    CHECK(back[1].state == State::Failed && back[1].error == b.error && back[1].unsigned_content && back[1].accept_unsigned);
    CHECK(!back[1].journal);
    // Nothing secret in it: only the server's id.
    CHECK(fs::read(queue_file()).find("password") == std::string::npos);

    // What a file edited by hand cannot hold.
    CHECK(queue_from_json("not json").empty());
    CHECK(queue_from_json(R"({"jobs": [{"id": "1", "url": "http://plain/x.nsp"}, {"url": "https://a/x.nsp"},
                             {"id": "2", "url": "https://a/x.nsp", "state": "elsewhere"}]})").size() == 1);
    const auto odd = queue_from_json(R"({"jobs": [{"id": "3", "url": "https://a/x.nsp", "journal":
        {"registered": ["zz", "0101010101010101010101010101010101"], "writing": "03030303030303030303030303030303",
         "offset": 99, "nca_size": 10, "sha": "00"}}]})");
    if (CHECK(odd.size() == 1 && odd[0].journal)) {
        CHECK(odd[0].journal->registered.empty());   // neither is a content id
        CHECK(odd[0].journal->writing == cid(3) && !odd[0].journal->at);   // a checkpoint that does not read
        CHECK(odd[0].state == State::Waiting);
    }
}

void test_queue()
{
    std::vector<Job> jobs = {job("1", State::Done, "https://a/1.nsp"), job("2", State::Paused, "https://a/2.nsp"),
                             job("3", State::Waiting, "https://a/3.nsp"), job("4", State::Waiting, "https://a/4.nsp"),
                             job("5", State::Failed, "https://a/5.nsp")};
    CHECK(next_job(jobs) == 2);
    CHECK(pending(jobs) == 3);
    CHECK(queued(jobs, "https://a/2.nsp") && queued(jobs, "https://a/3.nsp"));
    CHECK(!queued(jobs, "https://a/1.nsp") && !queued(jobs, "https://a/5.nsp") && !queued(jobs, "https://a/9.nsp"));
    jobs[2].state = jobs[3].state = State::Done;
    CHECK(next_job(jobs) == -1);
    CHECK(new_job_id().size() == 16 && new_job_id() != new_job_id());

    // The placeholders of jobs not done are kept; the others are orphans.
    jobs[1].journal = install::Journal{};
    jobs[1].journal->writing = cid(7);
    jobs[0].journal = install::Journal{};
    jobs[0].journal->writing = cid(8);   // done: its placeholder is no longer of use
    const auto lost = orphans({cid(7), cid(8), cid(9)}, jobs);
    CHECK(lost == std::vector<install::ContentId>({cid(8), cid(9)}));
}

void test_rate()
{
    Rate r;
    CHECK(r.bytes_per_second() == 0 && !r.eta(100));
    r.add(0, 0.0);
    r.add(500, 0.5);
    CHECK(r.bytes_per_second() == 0);   // under a second
    r.add(2000, 2.0);
    CHECK(r.bytes_per_second() == 1000);
    CHECK(r.eta(3000) && *r.eta(3000) == 3.0);
    // Only the last seconds count.
    for (int t = 3; t <= 12; t++) r.add(2000 + (uint64_t)(t - 2) * 4000, t);
    CHECK(r.bytes_per_second() > 3900 && r.bytes_per_second() <= 4000);
    // Another job: its count starts again.
    r.add(10, 13.0);
    CHECK(r.bytes_per_second() == 0);
}
}   // namespace

int main()
{
    test_file();
    test_queue();
    test_rate();
    return CHECK_DONE("download queue: its file and journals, a restart, the next job, orphan placeholders, the speed passed");
}
