// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/compress_flow.hpp"

#include <lz4.h>
#include <lz4hc.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <aurore.hpp>
#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/cadran/ui/cadran_modal.hpp>

#include "action/compress_logic.hpp"
#include "action/installed_flow.hpp"
#include "action/progress_dialog.hpp"
#include "action/rebuilt.hpp"
#include "install/compress_title.hpp"
#include "install/console_core.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/cpu_boost.hpp"
#include "util/paths.hpp"
#include "util/title_id.hpp"

using namespace aurore::literals;

namespace compress_flow
{
namespace
{
using namespace install;
using progress_dialog::mb_text;
using progress_dialog::Progress;

constexpr int LZ4_LEVEL = 6;

std::optional<size_t> lz4_compress(std::span<const uint8_t> in, std::span<uint8_t> out)
{
    const int n = LZ4_compress_HC((const char*)in.data(), (char*)out.data(), (int)in.size(), (int)out.size(), LZ4_LEVEL);
    if (n <= 0) return std::nullopt;
    return (size_t)n;
}

bool lz4_decompress(std::span<const uint8_t> in, std::span<uint8_t> out)
{
    return LZ4_decompress_safe((const char*)in.data(), (char*)out.data(), (int)in.size(), (int)out.size()) == (int)out.size();
}

// The journal on the SD card, written whole through a file beside it (fsync, then rename).
class SdJournal : public JournalStore
{
  public:
    static std::string path() { return paths::data_dir() + "/compress.journal"; }
    std::optional<std::string> load() override
    {
        std::string text;
        if (!paths::read_file(path(), text)) return std::nullopt;
        return text;
    }
    bool save(const std::string& text) override { return paths::atomic_write(path(), text); }
    void clear() override
    {
        std::remove(path().c_str());
        std::remove((path() + ".tmp").c_str());
        std::remove((path() + ".part").c_str());
    }
};

// What one compression holds between its steps: the console, the keys, the journal and the engine.
struct Session
{
    CoreConsole                      console{CoreStorage_Sd};
    NcaKeys                          keys;
    SdJournal                        journal;
    std::unique_ptr<TitleCompressor> engine;
    std::atomic<bool>                cancel{false};
    std::shared_ptr<Progress>        dialog;   // touched on the UI thread only
    std::atomic<int>                 percent{-1};
    uint64_t                         game = 0;
    std::string                      name;
};

const char* stage_text(CompressStage s)
{
    switch (s) {
        case CompressStage::Measuring: return "Measuring";
        case CompressStage::Writing: return "Writing the compressed copy";
        case CompressStage::Checking: return "Checking it against the original";
        case CompressStage::Reading: return "Reading it as the console does";
    }
    return "";
}

void log_failed(const std::string& what, const std::string& why)
{
    aurore::Logger::warning("compress: {}: {}", what, why);
}

// The user's decision on a compressed game that waits for one.
void decide(const std::string& name, const std::function<void()>& keep, const std::function<void()>& revert)
{
    ui::pick(std::format("{}: does it start?", name),
             { "Decide later (start the game first)", "Keep the compressed game (the old files are deleted)",
               "Go back to the original" },
             0, [keep, revert](int i) {
                 if (i == 1) keep();
                 else if (i == 2) revert();
             });
}

void keep_session(const std::shared_ptr<Session>& s)
{
    ui::in_background("compress-keep", [s]() {
        const Done d = s->engine->keep();
        aurore::sync([s, d]() {
            if (d) {
                rebuilt::note({ { s->game, true } });
                ui::info(std::format("{} stays compressed. The old files are deleted.", s->name));
            } else {
                log_failed("keep", d.error());
                ui::error("The compressed game could not be kept", d.error());
            }
        });
    });
}

void revert_session(const std::shared_ptr<Session>& s)
{
    ui::in_background("compress-revert", [s]() {
        const Done d = s->engine->revert();
        aurore::sync([s, d]() {
            if (d) ui::info(std::format("{} is as it was.", s->name));
            else {
                log_failed("revert", d.error());
                ui::error("Going back failed", d.error());
            }
        });
    });
}

void after_run(const std::shared_ptr<Session>& s)
{
    ui::info(std::format("{} is compressed. Start it from the HOME menu to try it, then come back here: PlayShelf will ask whether "
                         "to keep it. Until then the old files are kept.",
                         s->name));
    decide(s->name, [s]() { keep_session(s); }, [s]() { revert_session(s); });
}

void run_session(const std::shared_ptr<Session>& s)
{
    s->cancel = false;
    s->percent = -1;
    s->dialog = std::make_shared<Progress>("Compressing", s->name, &s->cancel);
    ui::in_background("compress-run", [s]() {
        cpu_boost::Guard boost(config::get().cpu_boost);
        const Done d = s->engine->run();
        aurore::sync([s, d]() {
            const bool cancelled = s->cancel.load();
            if (s->dialog) s->dialog->close();
            s->dialog.reset();
            if (d) after_run(s);
            else if (cancelled) ui::notify("Cancelled: the game is as it was.");
            else {
                log_failed("run", d.error());
                ui::error(std::format("{} could not be compressed", s->name), d.error() + "\n\nThe game is as it was.");
            }
        });
    });
}

void confirm_session(const std::shared_ptr<Session>& s, bool merges)
{
    const CompressReport& r = s->engine->report();
    ui::confirm(
        std::format("Compress {}?\n\nIts program goes from {} MB to {} MB ({}% less){}. The new copy needs {} MB free; {} MB are.\n\n"
                    "Only for a console whose loader is patched (sys-patch): the new copy cannot carry Nintendo's signature. "
                    "Official updates can no longer be installed over it, and nothing is deleted until you confirm the game "
                    "starts. This takes a long time; keep the console charged and awake.",
                    s->name, mb_text(r.program_before), mb_text(r.program_after), r.saving_percent(),
                    merges ? ", with the installed update merged in" : "", mb_text(r.needed), mb_text(r.free_space)),
        "Compress", [s]() { run_session(s); });
}

void prepare(const installed_flow::Game& g)
{
    auto s = std::make_shared<Session>();
    s->game = g.game.id;
    s->name = g.name.empty() ? title_id::text(g.game.id) : g.name;
    const bool merges = g.game.update != 0;
    s->dialog = std::make_shared<Progress>("Measuring", s->name, &s->cancel);
    const MetaKey base{ g.game.id, g.game.version, MetaType::Application };
    ui::in_background("compress-prepare", [s, base, merges, update = g.game.update]() {
        std::string why;
        if (!s->console.ok()) why = s->console.error();
        else if (const auto keys = core_keys(); !keys) why = keys.error();
        else {
            s->keys = *keys;
            CompressOptions options;
            if (merges) options.merge_update = MetaKey{ base.id | 0x800, update, MetaType::Patch };
            CompressCallbacks cb;
            cb.cancel = &s->cancel;
            cb.progress = [s](CompressStage stage, uint64_t done, uint64_t total) {
                const int now = total ? (int)std::min<uint64_t>(100, done * 100 / total) : 0;
                if (now == s->percent.exchange(now)) return;
                aurore::sync([s, stage, done, total]() {
                    if (s->dialog) s->dialog->show(stage_text(stage), done, total);
                });
            };
            s->engine = std::make_unique<TitleCompressor>(s->console, s->keys, s->journal, lz4_compress, lz4_decompress, base, options, cb);
            if (const Done d = s->engine->prepare(); !d) why = d.error();
        }
        aurore::sync([s, why, merges]() {
            const bool cancelled = s->cancel.load();
            if (s->dialog) s->dialog->close();
            s->dialog.reset();
            if (!why.empty()) {
                if (!cancelled) {
                    log_failed("prepare", why);
                    ui::error(std::format("{} cannot be compressed", s->name), why);
                }
                return;
            }
            confirm_session(s, merges);
        });
    });
}
}   // namespace

void open()
{
    if (ui::refuse_read_only()) return;
    const auto games = installed_flow::games();
    std::vector<size_t>      shown;
    std::vector<std::string> labels;
    if (games) {
        for (size_t i = 0; i < games->size(); i++) {
            const installed_flow::Game& g = (*games)[i];
            if (!g.game.base) continue;
            shown.push_back(i);
            labels.push_back((g.name.empty() ? title_id::text(g.game.id) : g.name) + (g.version.empty() ? "" : " · " + g.version) +
                             (g.game.update ? " (+ update)" : ""));
        }
    }
    if (shown.empty()) {
        ui::error("Compress an installed game", "No installed game is known yet: open the Library tab first.");
        return;
    }
    const patches::Report& report = ui::patch_report();
    if (const std::string why = compress_logic::signature_blocker(report); !why.empty()) {
        ui::error("Compress an installed game", why);
        return;
    }
    ui::pick("Compress which game?", labels, 0, [games, shown](int i) { prepare((*games)[shown[(size_t)i]]); });
}

void recover_at_start()
{
    if (!paths::exists_or_pending(SdJournal::path())) return;
    ui::in_background("compress-recover", []() {
        CoreConsole console(CoreStorage_Sd);
        SdJournal   journal;
        if (!console.ok()) {
            log_failed("recover", console.error());
            return;
        }
        const auto left = TitleCompressor::recover(console, journal);
        if (!left) {
            log_failed("recover", left.error());
            aurore::sync([why = left.error()]() { ui::error("A compression was left unfinished and could not be handled", why); });
            return;
        }
        const TitleCompressor::Left found = *left;
        aurore::sync([found]() {
            if (found.found == TitleCompressor::Found::CleanedUp) {
                ui::info("A compression the console interrupted was undone: the game is as it was.");
            } else if (found.found == TitleCompressor::Found::AwaitingDecision) {
                const std::string name = title_id::text(found.key.id);
                const uint64_t    id = found.key.id;
                decide(
                    name,
                    [id, name]() {
                        ui::in_background("compress-keep", [id, name]() {
                            CoreConsole sd(CoreStorage_Sd);
                            SdJournal   store;
                            const Done  d = sd.ok() ? TitleCompressor::keep_left(sd, store) : fail(sd.error());
                            aurore::sync([id, name, d]() {
                                if (d) {
                                    rebuilt::note({ { id, true } });
                                    ui::info(std::format("{} stays compressed. The old files are deleted.", name));
                                } else ui::error("The compressed game could not be kept", d.error());
                            });
                        });
                    },
                    [name]() {
                        ui::in_background("compress-revert", [name]() {
                            CoreConsole sd(CoreStorage_Sd);
                            SdJournal   store;
                            const Done  d = sd.ok() ? TitleCompressor::revert_left(sd, store) : fail(sd.error());
                            aurore::sync([name, d]() {
                                if (d) ui::info(std::format("{} is as it was.", name));
                                else ui::error("Going back failed", d.error());
                            });
                        });
                    });
            }
        });
    });
}

}   // namespace compress_flow
