// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/log_prune_flow.hpp"

#include <aurore.hpp>
#include <ctime>

#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/log_files.hpp"
#include "util/paths.hpp"

namespace log_prune_flow
{

void run()
{
    // Read here, on the UI thread, where config is changed.
    const auto& cfg = config::get();
    const log_files::Limits limits{ cfg.logs_keep, cfg.logs_keep_days, cfg.logs_keep_mb };
    ui::write_later("prune logs", [limits]() {
        const std::time_t now = std::time(nullptr);
        const size_t removed = log_files::prune(paths::reports_dir(), log_files::REPORT, limits, now) +
                               log_files::prune(paths::crashes_dir(), log_files::CRASH, limits, now) +
                               log_files::prune(paths::sessions_dir(), log_files::SESSION, limits, now) +
                               log_files::prune(paths::uploads_dir(), log_files::UPLOAD, limits, now);
        if (removed) aurore::Logger::info("logs: {} old report, crash, session or upload file(s) removed", removed);
    });
}

}   // namespace log_prune_flow
