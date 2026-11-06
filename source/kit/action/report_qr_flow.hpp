// report_qr_flow — the diagnostic report shown as QR codes (util/report_qr):
// four to a page, the pages one after the other, for a bug report that leaves
// the console by photographs, with no network and no account.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace report_qr_flow
{
// The report cut into codes and shown, or an error if it does not fit.
void open(const std::string& report);
}
