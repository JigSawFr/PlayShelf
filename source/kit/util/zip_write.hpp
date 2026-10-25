// zip_write — a zip archive with every file stored (no compression), which
// every XLSX reader accepts (util/table_export). The reading side is
// util/zip_read. Plain C++ (zlib's CRC-32).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace zip_write
{

// The archive of `files` (name, content), in that order, each dated `now`
// (local time, as zip keeps it).
std::string stored(const std::tm& now, const std::vector<std::pair<std::string, std::string>>& files);

}   // namespace zip_write
