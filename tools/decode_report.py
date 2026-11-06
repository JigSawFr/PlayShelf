#!/usr/bin/env python3
# Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
"""Put a diagnostic report back together from the QR codes it was shown as.

    tools/decode_report.py photo1.jpg photo2.jpg ...   # photos of the codes
    tools/decode_report.py pieces.txt                  # texts already decoded

Photos need `pip install zxing-cpp pillow`; unpacking needs Python 3.14 or
`pip install zstandard`. A piece is "PS1.<id>.<index>.<count>.<crc>:<base64>"
("PG" for PlayGuard): see source/kit/util/report_qr.hpp. Pieces of a newer
schema are skipped, a damaged one is named and left out; the report of each
complete set is written to report_<id>.txt (or to --out).
"""
import argparse
import base64
import re
import sys
import zlib

SCHEMA = 1
PIECE = re.compile(r"P[SG]\d{1,4}\.[0-9a-f]{8}\.\d{1,4}\.\d{1,4}\.[0-9a-f]{8}:[A-Za-z0-9+/=]*")
MAX_REPORT = 4 << 20


def unpack(data: bytes) -> bytes:
    try:
        from compression import zstd  # Python 3.14
        return zstd.decompress(data)
    except ImportError:
        pass
    try:
        import zstandard
    except ImportError:
        sys.exit("unpacking needs Python 3.14 or: pip install zstandard")
    return zstandard.ZstdDecompressor().decompress(data, max_output_size=MAX_REPORT)


def parse(piece: str):
    """(id, index, count, bytes), None for a piece to skip, or a string saying what is wrong."""
    head, _, body = piece.partition(":")
    fields = head.split(".")
    schema = re.fullmatch(r"P[SG](\d{1,4})", fields[0])
    if not schema:
        return None
    if int(schema.group(1)) > SCHEMA:
        return None   # a newer format: not ours to read
    if len(fields) != 5:
        return "wrong layout"
    _, pid, index, count, crc = fields
    index, count = int(index), int(count)
    if not (1 <= index <= count <= 64):
        return "index out of range"
    try:
        raw = base64.b64decode(body, validate=True)
    except ValueError:
        return "not base64"
    if zlib.crc32(raw) != int(crc, 16):
        return "CRC differs"
    return pid, index, count, raw


def pieces_in_text(text: str):
    return PIECE.findall(text)


def pieces_in_image(path: str):
    try:
        import zxingcpp
        from PIL import Image
    except ImportError:
        sys.exit("photos need: pip install zxing-cpp pillow")
    return [r.text for r in zxingcpp.read_barcodes(Image.open(path)) if r.text.startswith(("PS", "PG"))]


def decode(texts):
    """{id: report text} for each complete set; problems go to stderr."""
    sets = {}
    for t in texts:
        parsed = parse(t)
        if parsed is None:
            continue
        if isinstance(parsed, str):
            print(f"skipped a piece: {parsed}", file=sys.stderr)
            continue
        pid, index, count, raw = parsed
        entry = sets.setdefault(pid, {"count": count, "parts": {}})
        if entry["count"] != count:
            print(f"skipped a piece of {pid}: its count differs", file=sys.stderr)
            continue
        entry["parts"][index] = raw
    reports = {}
    for pid, entry in sets.items():
        missing = [i for i in range(1, entry["count"] + 1) if i not in entry["parts"]]
        if missing:
            print(f"report {pid}: pieces {missing} are missing (of {entry['count']})", file=sys.stderr)
            continue
        packed = b"".join(entry["parts"][i] for i in range(1, entry["count"] + 1))
        try:
            reports[pid] = unpack(packed).decode("utf-8", "replace")
        except Exception as e:  # a damaged frame: zstd names it
            print(f"report {pid}: does not unpack ({e})", file=sys.stderr)
    return reports


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("files", nargs="+", help="photos of the codes, or text files with their pieces")
    ap.add_argument("--out", help="write the report here (one complete report only)")
    args = ap.parse_args(argv)
    texts = []
    for f in args.files:
        if f.lower().endswith((".txt", ".log")):
            with open(f, encoding="utf-8", errors="replace") as fh:
                texts += pieces_in_text(fh.read())
        else:
            texts += pieces_in_image(f)
    reports = decode(texts)
    if not reports:
        print("no complete report found", file=sys.stderr)
        return 1
    if args.out and len(reports) > 1:
        print("several reports found: leave --out out", file=sys.stderr)
        return 1
    for pid, text in reports.items():
        path = args.out or f"report_{pid}.txt"
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(text)
        print(f"{path}: {len(text)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
