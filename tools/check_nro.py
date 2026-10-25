#!/usr/bin/env python3
"""Checks that a Switch binary was built with a libnx that knows the 21.0.0 TLS layout.

libnx 4.10.0 and later put an "LNY2" record after the MOD0 header of crt0
(MOD0 + 0x34: MOD0 is 0x1C bytes, then LNY0 and LNY1, 0xC each), with a
version word hbmenu reads: a binary without it, or below version 1, was built
before the 21.0.0 change of the thread-local storage and may corrupt memory
on 21.0.0 and later. The offset of MOD0 is the word at 4 of the image (crt0's
`.word __nx_mod0 - _start`).

Reads an .nro (its file is the memory image) or an .elf (mapped through its
program headers). An .nso is compressed: check the .elf it was made from.

The .nro files after --romfs must also carry a RomFS in their assets (ASET
after the NRO's code). elf2nro writes the assets only when it has an icon, a
NACP or a romfs image, not for --romfsdir alone: PlayGuard's PIN gate was once
built that way, without its font and with nothing after its code.

Usage: tools/check_nro.py <file.nro|file.elf>... [--romfs <file.nro>...]   (exit 1 when one fails)
"""
import struct
import sys

LNY2_OFFSET = 0x34
LNY2_MIN_VERSION = 1


def image_reader(data):
    """A function reading `size` bytes at a virtual address of the image."""
    if data[:4] != b"\x7fELF":
        return lambda addr, size: data[addr:addr + size]
    phoff, = struct.unpack_from("<Q", data, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", data, 0x36)
    loads = []
    for i in range(phnum):
        p_type, _, p_offset, p_vaddr, _, p_filesz = struct.unpack_from("<IIQQQQ", data, phoff + i * phentsize)
        if p_type == 1:   # PT_LOAD
            loads.append((p_vaddr, p_offset, p_filesz))

    def read(addr, size):
        for vaddr, offset, filesz in loads:
            if vaddr <= addr and addr + size <= vaddr + filesz:
                start = offset + addr - vaddr
                return data[start:start + size]
        return b""
    return read


def check(path):
    read = image_reader(open(path, "rb").read())
    head = read(4, 4)
    if len(head) != 4:
        return "too short for a Switch binary"
    mod0, = struct.unpack("<I", head)
    if read(mod0, 4) != b"MOD0":
        return "no MOD0 header at 0x%x" % mod0
    record = read(mod0 + LNY2_OFFSET, 8)
    if record[:4] != b"LNY2":
        return "no LNY2 marker after MOD0: built with libnx older than 4.10.0"
    version, = struct.unpack_from("<I", record, 4)
    if version < LNY2_MIN_VERSION:
        return "LNY2 version %d, below %d" % (version, LNY2_MIN_VERSION)
    print("%s: MOD0 at 0x%x, LNY2 version %d" % (path, mod0, version))
    return None


def check_romfs(path):
    """An .nro's assets: its RomFS there, inside the file."""
    data = open(path, "rb").read()
    if len(data) < 0x80 or data[0x10:0x14] != b"NRO0":
        return "not an .nro"
    size, = struct.unpack_from("<I", data, 0x18)
    if len(data) < size + 0x38 or data[size:size + 4] != b"ASET":
        return "no assets after its code (0x%x bytes, the file 0x%x): no RomFS" % (size, len(data))
    offset, length = struct.unpack_from("<QQ", data, size + 0x28)
    if length == 0 or size + offset + length > len(data):
        return "no RomFS in its assets"
    print("%s: RomFS of 0x%x bytes in its assets" % (path, length))
    return None


def main():
    if len(sys.argv) < 2:
        print(__doc__.strip().splitlines()[-1])
        return 2
    failed = False
    romfs = False
    for path in sys.argv[1:]:
        if path == "--romfs":
            romfs = True
            continue
        error = check(path) or (check_romfs(path) if romfs else None)
        if error:
            print("%s: %s" % (path, error), file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
