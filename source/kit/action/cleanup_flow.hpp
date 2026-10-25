// cleanup_flow — the developer tools' "Clear the cache" and "Delete the log
// files" (util/data_cleanup.hpp says which files): the folder is read on the
// SD-card write queue, a confirmation says how much goes and what it is, and
// the files are removed on the same queue, a toast saying how many. Nothing
// here touches the console's settings: allowed in read-only mode.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace cleanup_flow
{

void clear_cache();
void clear_logs();

}   // namespace cleanup_flow
