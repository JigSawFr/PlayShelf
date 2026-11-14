// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/check_flow.hpp"

#include <algorithm>
#include <atomic>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <aurore.hpp>

#include "action/progress_dialog.hpp"
#include "install/console_core.hpp"
#include "install/verify_title.hpp"
#include "library/catalog.hpp"
#include "library/repair_plan.hpp"
#include "ui/ui.hpp"

using namespace aurore::literals;

namespace check_flow
{
namespace
{
using namespace install;

constexpr size_t LINES_SHOWN = 8;

// What a game is made of on the console: its base game, its update and its add-ons, each at the version
// the console holds.
std::vector<MetaKey> keys_of(uint64_t game, const std::map<uint64_t, uint32_t>& installed)
{
    std::vector<MetaKey> keys;
    for (const auto& [id, version] : installed) {
        if (library::base_of(id) != game) continue;
        switch (library::kind_of(id)) {
            case library::Kind::Base: keys.push_back({id, version, MetaType::Application}); break;
            case library::Kind::Update: keys.push_back({id, version, MetaType::Patch}); break;
            case library::Kind::Dlc: keys.push_back({id, version, MetaType::AddOnContent}); break;
            default: break;
        }
    }
    return keys;
}

std::string state_text(ContentState s)
{
    switch (s) {
        case ContentState::Missing: return "playshelf/check/st_missing"_i18n;
        case ContentState::SizeWrong: return "playshelf/check/st_size"_i18n;
        case ContentState::Unreadable: return "playshelf/check/st_unreadable"_i18n;
        case ContentState::Corrupt: return "playshelf/check/st_corrupt"_i18n;
        case ContentState::Ok: break;
    }
    return "";
}

// "1a2b3c4d…" (8 hex digits): enough to tell the files apart in a list.
std::string short_id(const ContentId& id)
{
    return content_id_hex(id).substr(0, 8) + "…";
}

// A title of the game with damaged files: its key, and the contents to write again.
struct DamagedTitle
{
    MetaKey                key;
    std::vector<ContentId> contents;
};

struct Outcome
{
    std::string                 error;       // the check could not be done
    bool                        here = true; // the game is on the SD card
    size_t                      files = 0;
    std::vector<ContentVerdict> damaged;
    std::vector<DamagedTitle>   titles;
};

library::Kind kind_of(MetaType t)
{
    switch (t) {
        case MetaType::Application: return library::Kind::Base;
        case MetaType::Patch: return library::Kind::Update;
        case MetaType::AddOnContent: return library::Kind::Dlc;
        default: return library::Kind::Unknown;
    }
}

// The damaged files deleted (the installer writes the contents the console lacks), then their packages
// queued as a repair. A package the library does not have stops it before anything is deleted.
void repair(const std::shared_ptr<const library_flow::Loaded>& data, const std::string& name, std::vector<size_t> items,
            std::vector<ContentId> contents)
{
    ui::in_background("repair game", [data, name, items = std::move(items), contents = std::move(contents)]() {
        CoreConsole console(CoreStorage_Sd);
        std::string error;
        if (!console.ok()) error = console.error();
        else
            for (const ContentId& id : contents) console.delete_content(id);
        aurore::sync([data, name, items, error]() {
            if (!error.empty()) {
                ui::error("playshelf/check/action"_i18n, aurore::getStr("playshelf/check/failed", error));
                return;
            }
            library_flow::install(*data, items, true);
            ui::info(aurore::getStr("playshelf/check/repair_queued", name));
        });
    });
}

struct Run
{
    std::atomic<bool>                        cancel{false};
    std::shared_ptr<progress_dialog::Progress> dialog;   // touched on the UI thread only
    std::atomic<int>                         percent{-1};
};

void report(const std::string& name, const Outcome& o, const std::shared_ptr<const library_flow::Loaded>& data, size_t index)
{
    if (!o.error.empty()) {
        ui::error("playshelf/check/action"_i18n, aurore::getStr("playshelf/check/failed", o.error));
        return;
    }
    if (!o.here) {
        ui::info(aurore::getStr("playshelf/check/not_here", name));
        return;
    }
    if (o.damaged.empty()) {
        ui::info(aurore::getStr("playshelf/check/ok", name, o.files));
        return;
    }
    std::string lines;
    for (size_t i = 0; i < o.damaged.size() && i < LINES_SHOWN; i++) {
        const ContentVerdict& v = o.damaged[i];
        lines += std::format("{}• {} ({} MB): {}", i ? "\n" : "", short_id(v.id), progress_dialog::mb_text(v.size), state_text(v.state));
    }
    if (o.damaged.size() > LINES_SHOWN) lines += aurore::getStr("playshelf/check/more", o.damaged.size() - LINES_SHOWN);
    // The library has the packages: the repair is offered.
    if (data && index < data->titles.size()) {
        std::vector<library::Damaged> damaged;
        std::vector<ContentId>        contents;
        for (const DamagedTitle& t : o.titles) {
            damaged.push_back({t.key.id, t.key.version, kind_of(t.key.type)});
            contents.insert(contents.end(), t.contents.begin(), t.contents.end());
        }
        const library::RepairPlan plan = library::plan_repair(data->titles[index], data->fetch.catalog.items, damaged);
        if (plan.missing.empty() && !plan.items.empty()) {
            ui::confirm(
                aurore::getStr("playshelf/check/bad_repair", name, o.damaged.size(), o.files, lines, progress_dialog::mb_text(plan.bytes)),
                "playshelf/check/repair_go"_i18n,
                [data, name, items = plan.items, contents]() { repair(data, name, items, contents); }, []() {});
            return;
        }
    }
    ui::error("playshelf/check/action"_i18n, aurore::getStr("playshelf/check/bad", name, o.damaged.size(), o.files, lines));
}

void start(uint64_t game, const std::string& name, const std::shared_ptr<const library_flow::Loaded>& data, size_t index)
{
    auto run = std::make_shared<Run>();
    run->dialog = std::make_shared<progress_dialog::Progress>("playshelf/check/heading"_i18n, name, &run->cancel);
    ui::in_background("check game", [run, game, name, data, index]() {
        Outcome outcome;
        CoreConsole console(CoreStorage_Sd);
        if (!console.ok()) {
            outcome.error = console.error();
        } else if (const auto installed = installed_versions(); !installed) {
            outcome.error = installed.error();
        } else {
            const std::vector<MetaKey> keys = keys_of(game, *installed);
            for (const MetaKey& key : keys) {
                if (run->cancel) break;
                const auto verdict = verify_title(
                    console, key,
                    [run](uint64_t done, uint64_t total) {
                        const int pct = total ? (int)(done * 100 / total) : 100;
                        if (pct == run->percent.exchange(pct)) return;   // at most once a percent
                        aurore::sync([run, done, total]() {
                            if (run->dialog) run->dialog->show("playshelf/check/heading"_i18n, done, total);
                        });
                    },
                    &run->cancel);
                if (!verdict) {
                    if (verdict.error() == "cancelled") break;
                    // A title that is not on the SD card (the internal memory's) has no record here.
                    if (key.type == MetaType::Application) outcome.here = false;
                    else outcome.error = verdict.error();
                    continue;
                }
                outcome.files += verdict->contents.size();
                if (!verdict->damaged().empty()) outcome.titles.push_back({key, verdict->damaged()});
                for (const ContentVerdict& v : verdict->contents)
                    if (v.state != ContentState::Ok) outcome.damaged.push_back(v);
            }
            if (keys.empty()) outcome.here = false;
        }
        aurore::sync([run, name, outcome, data, index]() {
            const bool cancelled = run->cancel;
            if (run->dialog) run->dialog->close();
            run->dialog.reset();
            if (!cancelled) report(name, outcome, data, index);
        });
    });
}
}   // namespace

void open(uint64_t game, const std::string& name, std::shared_ptr<const library_flow::Loaded> data, size_t index)
{
    ui::confirm(
        aurore::getStr("playshelf/check/body", name), "playshelf/check/go"_i18n, [game, name, data, index]() { start(game, name, data, index); },
        []() {});
}

}   // namespace check_flow
