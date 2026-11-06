// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

#include "check.h"
#include "util/report_qr.hpp"

using namespace report_qr;

static std::string noisy(size_t n, unsigned seed)
{
    std::mt19937 g(seed);
    std::string  s;
    while (s.size() < n) s += "t=" + std::to_string(g()) + " line " + std::to_string(g() % 977) + " of the log\n";
    return s;
}

int main()
{
    // CRC-32 of "123456789" is cbf43926.
    CHECK(crc32("123456789") == 0xcbf43926u);

    // A small report in one piece, and back.
    const std::string head = "playshelf 1.2.3\nfirmware 19.0.1\n\n";
    Encoded e = encode(head, "last line\n", 0xabc12345u);
    CHECK(e.pieces.size() == 1 && !e.trimmed);
    CHECK(e.pieces[0].rfind("PS1.abc12345.1.1.", 0) == 0);
    Reader r;
    CHECK(r.add(e.pieces[0]) == Reader::Add::Ok && r.complete());
    CHECK(r.text() && *r.text() == head + "last line\n");

    // Several pieces, in any order, with a duplicate; incomplete gives nothing.
    const std::string tail = noisy(4000, 1);
    Options opt;
    opt.chunk_bytes = 200;
    opt.max_chunks = 40;
    e = encode(head, tail, 7, opt);
    CHECK(e.pieces.size() > 2 && !e.trimmed);
    std::vector<std::string> shuffled = e.pieces;
    std::mt19937 g(5);
    std::shuffle(shuffled.begin(), shuffled.end(), g);
    Reader r2;
    CHECK(r2.add(shuffled[0]) == Reader::Add::Ok && r2.add(shuffled[0]) == Reader::Add::Duplicate);
    CHECK(!r2.complete() && !r2.text());
    for (size_t i = 1; i < shuffled.size(); i++) CHECK(r2.add(shuffled[i]) == Reader::Add::Ok);
    CHECK(r2.complete() && r2.have() == e.pieces.size() && r2.total() == e.pieces.size());
    CHECK(r2.text() && *r2.text() == head + tail);
    const Encoded     fixture = e;
    const std::string fixture_text = head + tail;

    // Too many pieces: the start of the tail goes first, the head stays.
    opt.max_chunks = 4;
    e = encode(head, noisy(60000, 2), 8, opt);
    CHECK(!e.pieces.empty() && e.pieces.size() <= 4 && e.trimmed);
    Reader r3;
    for (const auto& p : e.pieces) CHECK(r3.add(p) == Reader::Add::Ok);
    const auto text = r3.text();
    CHECK(text && text->rfind(head + "[...]\n", 0) == 0);
    // A head that cannot fit gives no pieces at all.
    opt.max_chunks = 1;
    CHECK(encode(noisy(60000, 3), "", 9, opt).pieces.empty());

    // A damaged piece, a piece of another report, a newer schema, not ours.
    std::string bad = e.pieces[0];
    bad[bad.size() - 3] = bad[bad.size() - 3] == 'A' ? 'B' : 'A';
    Reader r4;
    CHECK(r4.add(bad) == Reader::Add::Bad && r4.total() == 0);
    CHECK(r4.add(e.pieces[0]) == Reader::Add::Ok);
    Encoded other = encode(head, tail, 99, opt);
    CHECK(r4.add(other.pieces[0]) == Reader::Add::OtherReport);
    std::string newer = e.pieces[0];
    newer[2] = '2';
    CHECK(Reader().add(newer) == Reader::Add::Ignored);
    CHECK(Reader().add("https://example.org/") == Reader::Add::Ignored);
    CHECK(Reader().add("") == Reader::Add::Ignored);
    CHECK(Reader().add("PS1.zz.1.1.00000000:AAAA") == Reader::Add::Bad);
    CHECK(Reader().add("PS1.00000001.2.1.00000000:") == Reader::Add::Bad);

    // PlayGuard's magic is read too.
    Options pg;
    pg.magic = "PG";
    Encoded guard = encode(head, "x\n", 3, pg);
    Reader r5;
    CHECK(guard.pieces[0].rfind("PG1.", 0) == 0 && r5.add(guard.pieces[0]) == Reader::Add::Ok && r5.text());

    // A piece that is intact but not a zstd frame: complete, yet no text.
    Reader r6;
    const std::string raw = "not zstd";
    char crc[16];
    std::snprintf(crc, sizeof crc, "%08x", crc32(raw));
    CHECK(r6.add(std::string("PS1.00000001.1.1.") + crc + ":bm90IHpzdGQ=") == Reader::Add::Ok);
    CHECK(r6.complete() && !r6.text());

    // The pieces of a report for tests/decode_report/test.py (the other reader),
    // when REPORT_QR_FIXTURE names a file: one piece per line, the report after "---".
    if (const char* path = std::getenv("REPORT_QR_FIXTURE")) {
        if (FILE* f = std::fopen(path, "w")) {
            for (const auto& p : fixture.pieces) std::fprintf(f, "%s\n", p.c_str());
            std::fprintf(f, "---\n%s", fixture_text.c_str());
            std::fclose(f);
        }
    }
    CHECK_DONE("report_qr");
}
