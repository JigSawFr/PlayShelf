// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/homebrew_flow.hpp"

#include <atomic>
#include <cstdio>

#include <aurore.hpp>

#include "lantern_app/delta.hpp"
#include "lantern_app/package.hpp"
#include "lantern_app/platform.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"

using namespace aurore::literals;

namespace homebrew_flow
{

namespace
{
std::shared_ptr<const Loaded> s_loaded;
homebrew::Book                s_book;
bool                          s_book_read = false;
bool                          s_loading = false, s_busy = false;

std::string book_file() { return paths::data_dir() + "/homebrew.json"; }
std::string package_file() { return paths::data_dir() + "/homebrew.pkg"; }
std::string staging_dir() { return paths::data_dir() + "/homebrew-staging"; }

void read_book()
{
    if (s_book_read) return;
    s_book_read = true;
    s_book.load(book_file());
}
}   // namespace

std::shared_ptr<const Loaded> loaded() { return s_loaded; }

const homebrew::Book& book()
{
    read_book();
    return s_book;
}

bool busy() { return s_busy; }

Standing standing(const lantern_app::HomebrewItem& item)
{
    const homebrew::Record* r = book().find(std::string(item.entry.id));
    if (!r) return Standing::New;
    return item.entry.build > r->build ? Standing::Update : Standing::Installed;
}

void load(const library::Server& server, const std::function<void(std::shared_ptr<const Loaded>, std::string)>& done)
{
    if (s_loading) return;
    s_loading = true;
    ui::in_background("homebrew", [server, done]() {
        lantern_app::Platform platform;
        auto                  data = std::make_shared<Loaded>();
        data->server = server;
        std::string why;
        auto        c = lantern_app::connect(server, platform.env(), platform.store());
        if (c.status != lantern_app::Status::Ok) why = c.why.empty() ? "not paired" : c.why;
        else why = lantern_app::fetch_homebrew(*c.client, platform.env().platform, c.endpoint, "", data->list);
        aurore::sync([data, why, done]() {
            s_loading = false;
            if (why.empty()) s_loaded = data;
            done(why.empty() ? data : nullptr, why);
        });
    });
}

void install(const std::shared_ptr<const Loaded>& data, size_t index, const std::function<void(std::string)>& done)
{
    if (s_busy || !data || index >= data->list.items.size()) return;
    read_book();
    s_busy = true;
    const lantern_app::HomebrewItem item = data->list.items[index];
    const library::Server           server = data->server;
    const homebrew::Record*         old = s_book.find(std::string(item.entry.id));
    const std::optional<homebrew::Record> old_copy = old ? std::optional<homebrew::Record>(*old) : std::nullopt;

    // The paths that need consent are asked on the screen first.
    std::vector<std::string> asks;
    if (const auto package = lantern::pick_package(item.entry, ""))
        if (const auto plan = lantern::plan_install(item.entry, **package))
            for (const auto& f : plan->files)
                if (f.needs_consent) asks.emplace_back(f.path);
    const auto go = [item, server, old_copy, done]() {
        ui::in_background("homebrew install", [item, server, old_copy, done]() {
            lantern_app::Platform platform;
            lantern_app::PackageRequest request;
            request.url = item.url;
            request.size = item.size;
            request.sha256 = lantern::to_hex((*lantern::pick_package(item.entry, ""))->sha256).data();
            request.sha256.resize(64);
            request.peers = lantern_app::peers_of(server.uid);
            std::remove(package_file().c_str());
            std::string why;
            // An update of a single file reads the chunks the old copy
            // already holds (SPEC.md 22.3) and asks for the rest.
            lantern_app::DeltaResult got;
            const auto package = lantern::pick_package(item.entry, "");
            std::vector<std::string> seeds;
            if (old_copy && package && !(*package)->path.empty() && (*package)->chunks) {
                const std::string path((*package)->path);
                for (const std::string& f : old_copy->files)
                    if (f == path) seeds.push_back(paths::sd_root() + path);
            }
            if (!seeds.empty()) {
                const auto& ch = *(*package)->chunks;
                std::string sha(lantern::to_hex(ch.sha256).data(), 64);
                got = lantern_app::fetch_package_delta(platform, server, request, item.chunks_url, ch.size, sha, seeds,
                                                       package_file(), nullptr, nullptr);
            } else {
                static_cast<lantern_app::PackageResult&>(got) =
                    lantern_app::fetch_package(platform, server, request, package_file(), nullptr, nullptr);
            }
            homebrew::Outcome out;
            if (!got.ok) why = got.why;
            else {
                out = homebrew::install(item.entry, package_file(), paths::sd_root(), staging_dir(),
                                        old_copy ? &*old_copy : nullptr, [](const std::vector<std::string>&) { return true; });
                if (!out.ok) why = out.why;
            }
            std::remove(package_file().c_str());
            aurore::sync([out, why, done, uid = server.uid]() {
                s_busy = false;
                if (why.empty()) {
                    homebrew::Record record = out.record;
                    record.server_uid = uid;
                    s_book.put(record);
                    if (!s_book.newer) s_book.save(book_file());
                }
                done(why);
            });
        });
    };
    if (asks.empty()) {
        go();
        return;
    }
    std::string body = aurore::getStr("playshelf/homebrew/consent_body", item.name);
    for (const std::string& p : asks) body += "\n" + p;
    ui::confirm(body, "playshelf/homebrew/consent_yes"_i18n, go, [done]() {
        s_busy = false;   // declined: nothing was done
        done(DECLINED);
    });
}

std::string remove(const std::string& id)
{
    read_book();
    const homebrew::Record* r = s_book.find(id);
    if (!r) return {};
    const homebrew::Record copy = *r;
    const auto left = homebrew::uninstall(copy, paths::sd_root());
    s_book.erase(id);
    if (!s_book.newer && !s_book.save(book_file())) return "homebrew.json could not be written";
    return left.empty() ? std::string() : "some files could not be removed";
}

}   // namespace homebrew_flow
