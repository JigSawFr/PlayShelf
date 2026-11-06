#!/usr/bin/env python3
# Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
"""tools/decode_report.py reads what the C++ encoder wrote.

    python3 tests/decode_report/test.py <fixture>   # REPORT_QR_FIXTURE=<fixture> build/host-tests/report_qr

Skipped (exit 0) when the fixture or a zstd reader is missing.
"""
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import decode_report as d  # noqa: E402


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else ""
    if not os.path.exists(path):
        print("decode_report: no fixture, skipped")
        return 0
    try:
        d.unpack(b"")
    except SystemExit:
        print("decode_report: no zstd reader, skipped")
        return 0
    except Exception:
        pass
    text = open(path, encoding="utf-8").read()
    pieces_text, _, expected = text.partition("---\n")
    pieces = d.pieces_in_text(pieces_text)
    assert len(pieces) > 2, pieces

    # Whole set, shuffled, with a duplicate.
    shuffled = pieces + [pieces[0]]
    random.Random(3).shuffle(shuffled)
    reports = d.decode(shuffled)
    assert list(reports.values()) == [expected], "the report differs"

    # A missing piece gives nothing; a damaged one is skipped.
    assert d.decode(pieces[1:]) == {}
    damaged = pieces[0][:-3] + ("A" if pieces[0][-3] != "A" else "B") + pieces[0][-2:]
    assert d.decode([damaged] + pieces[1:]) == {}
    assert list(d.decode([damaged] + pieces).values()) == [expected]

    # A newer schema or text that is not ours is skipped without a word.
    newer = "PS2" + pieces[0][3:]
    assert d.parse(newer) is None and d.parse("https://example.org/") is None
    assert list(d.decode([newer] + pieces).values()) == [expected]

    # Pieces found in a file with other text around them.
    assert d.pieces_in_text("noise\n" + "\n  ".join(pieces) + "\nmore") == pieces
    print("decode_report: passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
