// log_prune_flow — removes the saved reports, the crash files, the session
// logs and the links of the reports sent online past the limits of About › Help & diagnostics (config logs_keep, logs_keep_days,
// logs_keep_mb; util/log_files.hpp), on the SD-card write queue. At start-up
// and when a limit changes.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace log_prune_flow
{

void run();

}   // namespace log_prune_flow
