// cadran_text — the words the Cadran screens lay out, with no view: a
// dialog's table, a week's identical lines as one, a failure's title, reason
// and code. Translated through ui/tr.hpp, so a host suite checks them against
// the real catalogs (tests/cadran_text).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace cadran_text
{

// A dialog paragraph that is a table, as its caller built it: the rows, each
// a label and its value, after a mark that tells cadran::dialog_body (no
// guessing from the words). `table_rows` reads one back: false for a
// paragraph that is not one.
using Rows = std::vector<std::pair<std::string, std::string>>;
std::string table(const Rows& rows);
bool table_rows(const std::string& paragraph, Rows* rows);

// A table of five lines or more (a week): the lines that follow each other
// with the same value as one, their labels joined with " – " ("Monday –
// Friday"). Fewer lines are kept as they are.
std::vector<std::pair<std::string, std::string>> group_rows(const std::vector<std::pair<std::string, std::string>>& rows);

// A failure as its dialog shows it (ui::error): what failed as the title, why
// as the text (the error's hint, first letter in capitals), the code small
// ("error 0x0000C88E", playshelf/error/code) and Send a report when the
// console's own code says nothing more (not one of PlayShelf's results).
struct Failure
{
    std::string title;    // "" when there is none
    std::string reason;
    std::string code;     // "" for none
    bool report = false;
};
// `what` failed with `rc`; `hint` its words (playshelf/error/<key>, "" when
// none is known); `app_result`: one of PlayShelf's own results (NXM_*),
// which explain themselves and have no code to show.
Failure failure(const std::string& what, const std::string& hint, uint32_t rc, bool app_result);

}   // namespace cadran_text
