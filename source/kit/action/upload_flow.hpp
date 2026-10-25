// upload_flow — Help & diagnostics › Send a report online: pick a report, sign in to
// GitHub first when PlayShelf is not (the report goes to a secret gist in
// that account), confirm what goes, send it (util/log_upload.hpp), then show
// its link and two QR codes: the report itself, and the bug-report form with
// the link already in it. A list (the games to reinstall) goes the same way,
// with its link alone.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace upload_flow
{

// Offers this report with the debug files, or one saved in logs/.
void choose();

// Sends `report` (as the diagnostic screen shows it) with the debug files,
// after the same confirmation.
void send_report(const std::string& report);

// Sends a list, `text`, as the file `name` ("games_to_reinstall.txt") of a
// secret gist described as `what` ("games to reinstall"), after the same
// sign-in and confirmation; then its link and its QR code.
void send_list(const std::string& name, const std::string& text, const std::string& what);

}   // namespace upload_flow
