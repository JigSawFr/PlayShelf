// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "ui/cadran_text.hpp"

#include <fmt/format.h>

#include "ui/tr.hpp"

using namespace brls::literals;

namespace cadran_text
{

namespace
{
// ASCII's record and unit separators: in no catalog's words.
constexpr char TABLE_MARK = '\x1E';
constexpr char CELL_MARK  = '\x1F';
}   // namespace

std::string table(const Rows& rows)
{
    std::string out(1, TABLE_MARK);
    for (size_t i = 0; i < rows.size(); i++)
        out += (i ? "\n" : "") + rows[i].first + CELL_MARK + rows[i].second;
    return out;
}

bool table_rows(const std::string& paragraph, Rows* rows)
{
    if (paragraph.empty() || paragraph[0] != TABLE_MARK) return false;
    rows->clear();
    size_t from = 1;
    while (from <= paragraph.size()) {
        size_t end = paragraph.find('\n', from);
        if (end == std::string::npos) end = paragraph.size();
        const std::string line = paragraph.substr(from, end - from);
        const size_t cell = line.find(CELL_MARK);
        if (cell != std::string::npos) rows->push_back({ line.substr(0, cell), line.substr(cell + 1) });
        from = end + 1;
    }
    return !rows->empty();
}

std::vector<std::pair<std::string, std::string>> group_rows(const std::vector<std::pair<std::string, std::string>>& rows)
{
    if (rows.size() < 5) return rows;
    std::vector<std::pair<std::string, std::string>> runs;
    for (size_t i = 0; i < rows.size();) {
        size_t j = i;
        while (j + 1 < rows.size() && rows[j + 1].second == rows[i].second) j++;
        runs.push_back({ j > i ? rows[i].first + " – " + rows[j].first : rows[i].first, rows[i].second });
        i = j + 1;
    }
    return runs;
}

Failure failure(const std::string& what, const std::string& hint, uint32_t rc, bool app_result)
{
    Failure f;
    f.title = what;
    f.reason = hint;
    if (!f.reason.empty() && f.reason[0] >= 'a' && f.reason[0] <= 'z') f.reason[0] = (char)(f.reason[0] - 'a' + 'A');
    if (!app_result) {
        f.code = brls::getStr("playshelf/error/code", fmt::format("0x{:08X}", (unsigned)rc));
        f.report = true;
    }
    return f;
}

}   // namespace cadran_text
