// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/zip_write.hpp"

#include <algorithm>
#include <cstdint>
#include <zlib.h>

namespace zip_write
{

namespace
{
// zlib's CRC-32 of a part (a sheet is far below 4 GiB).
uint32_t crc_of(const std::string& data)
{
    return (uint32_t)crc32(0L, (const Bytef*)data.data(), (uInt)data.size());
}

void put16(std::string& o, uint32_t v)
{
    o += (char)(v & 0xFF);
    o += (char)((v >> 8) & 0xFF);
}

void put32(std::string& o, uint32_t v)
{
    put16(o, v & 0xFFFF);
    put16(o, v >> 16);
}
}   // namespace

std::string stored(const std::tm& now, const std::vector<std::pair<std::string, std::string>>& files)
{
    const uint32_t dos_time = ((uint32_t)now.tm_hour << 11) | ((uint32_t)now.tm_min << 5) | ((uint32_t)now.tm_sec / 2);
    const uint32_t dos_date = ((uint32_t)std::max(0, now.tm_year - 80) << 9) | ((uint32_t)(now.tm_mon + 1) << 5) |
                              (uint32_t)now.tm_mday;
    std::string out, central;
    for (const auto& f : files) {
        const uint32_t crc = crc_of(f.second), size = (uint32_t)f.second.size(), offset = (uint32_t)out.size();
        put32(out, 0x04034B50); put16(out, 20); put16(out, 0); put16(out, 0);   // version, flags, stored
        put16(out, dos_time); put16(out, dos_date);
        put32(out, crc); put32(out, size); put32(out, size);
        put16(out, (uint32_t)f.first.size()); put16(out, 0);
        out += f.first + f.second;

        put32(central, 0x02014B50); put16(central, 20); put16(central, 20); put16(central, 0); put16(central, 0);
        put16(central, dos_time); put16(central, dos_date);
        put32(central, crc); put32(central, size); put32(central, size);
        put16(central, (uint32_t)f.first.size()); put16(central, 0); put16(central, 0);   // name, extra, comment
        put16(central, 0); put16(central, 0); put32(central, 0);                           // disk, attributes
        put32(central, offset);
        central += f.first;
    }
    const uint32_t central_offset = (uint32_t)out.size();
    out += central;
    put32(out, 0x06054B50); put16(out, 0); put16(out, 0);
    put16(out, (uint32_t)files.size()); put16(out, (uint32_t)files.size());
    put32(out, (uint32_t)central.size()); put32(out, central_offset); put16(out, 0);
    return out;
}

}   // namespace zip_write
