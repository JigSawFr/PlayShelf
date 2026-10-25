// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/cleanup_flow.hpp"

#include <borealis.hpp>
#include <functional>
#include <memory>
#include <string>

#include "ui/ui.hpp"
#include "util/data_cleanup.hpp"
#include "util/paths.hpp"
#include "util/session_log.hpp"

using namespace brls::literals;

namespace cleanup_flow
{

namespace
{
// "6 files (2 MB)", in the app's language.
std::string files_text(size_t count, uint64_t bytes)
{
    uint64_t value = 0;
    const bool mb = data_cleanup::size_unit(bytes, &value) == data_cleanup::Unit::MB;
    const std::string size = brls::getStr(mb ? "playshelf/dev/size_mb" : "playshelf/dev/size_kb", std::to_string(value));
    if (count == 1) return brls::getStr("playshelf/dev/files_one", size);
    return brls::getStr("playshelf/dev/files", std::to_string(count), size);
}

// Lists the files on the write queue (behind the writes asked before, and
// off the UI thread: the icons are a few hundred files), then asks, then
// says what went.
void run(const char* what, std::function<data_cleanup::Plan()> make_plan, const std::string& body_key)
{
    ui::write_later(what, [what, make_plan, body_key]() {
        auto plan = std::make_shared<data_cleanup::Plan>(make_plan());
        brls::sync([what, plan, body_key]() {
            if (plan->files.empty()) {
                ui::info("playshelf/dev/clear_empty"_i18n);
                return;
            }
            ui::confirm_danger(brls::getStr(body_key, files_text(plan->files.size(), plan->bytes)),
                               "playshelf/common/delete"_i18n, [what, plan]() {
                ui::write_later(what, [what, plan]() {
                    const data_cleanup::Outcome out = data_cleanup::remove(*plan);
                    brls::Logger::info("cleanup ({}): {} file(s) removed, {} bytes, {} not removed", what, out.removed,
                                       out.bytes, out.failed);
                    brls::sync([out]() {
                        const std::string done = files_text(out.removed, out.bytes);
                        if (out.failed) ui::error(brls::getStr("playshelf/dev/clear_failed", done, std::to_string(out.failed)));
                        else ui::info(brls::getStr("playshelf/dev/cleared", done));
                    });
                });
            });
        });
    });
}
}   // namespace

void clear_cache()
{
    run("clear cache", []() { return data_cleanup::cache(paths::data_dir()); }, "playshelf/dev/clear_cache_body");
}

void clear_logs()
{
    run("clear logs", []() { return data_cleanup::logs(paths::data_dir(), session_log::current_name()); },
        "playshelf/dev/clear_logs_body");
}

}   // namespace cleanup_flow
