// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/peer_flow.hpp"

#include <memory>
#include <mutex>

#include <aurore.hpp>

#include "lantern_app/peer_book.hpp"
#include "lantern_app/peer_service.hpp"
#include "lantern_app/system.hpp"
#include "util/config.hpp"
#include "util/paths.hpp"

namespace peer_flow
{

namespace
{
std::mutex                                    s_lock;
lantern_app::PeerBook                         s_book;
bool                                          s_loaded = false;
lantern_app::SystemClock                      s_clock;
std::unique_ptr<lantern_app::PeerResponder>   s_responder;
std::unique_ptr<lantern_app::PeerServer>      s_server;

std::string book_file() { return paths::data_dir() + "/peers.json"; }

void load_locked()
{
    if (s_loaded) return;
    s_loaded = true;
    s_book.load(book_file());
}

void save_locked()
{
    if (!s_book.newer && !s_book.save(book_file())) aurore::Logger::warning("peers.json could not be written");
}

void stop_locked()
{
    s_server.reset();
    s_responder.reset();
}

// The server runs on the book as it is now, or not at all.
void rebuild_locked()
{
    stop_locked();
    if (!config::get().share_files || s_book.newer) return;
    if (s_book.uid().empty()) {
        uint8_t bytes[16] = {};
        lantern_app::SystemRandom random;
        if (!random.fill(std::span<uint8_t>(bytes, 16))) return;
        s_book.set_uid(lantern_app::uuid_from(bytes));
        save_locked();
    }
    s_responder = std::make_unique<lantern_app::PeerResponder>(s_book, paths::sd_root(), "PlayShelf", s_clock);
    s_responder->set_enabled(true);
    s_server = std::make_unique<lantern_app::PeerServer>(*s_responder, lantern::discovery_port, lantern_app::PEER_PORT);
    const std::string why = s_server->start();
    if (!why.empty()) {
        aurore::Logger::warning("sharing files: {}", why);
        stop_locked();
    }
}
}   // namespace

void apply()
{
    const std::lock_guard<std::mutex> guard(s_lock);
    load_locked();
    rebuild_locked();
}

void shutdown()
{
    const std::lock_guard<std::mutex> guard(s_lock);
    stop_locked();
}

void note_server(const std::string& server_uid, const std::string& key_hex)
{
    const std::lock_guard<std::mutex> guard(s_lock);
    load_locked();
    const auto it = s_book.servers().find(server_uid);
    if (it != s_book.servers().end() && it->second == key_hex) return;
    s_book.set_server(server_uid, key_hex);
    save_locked();
    if (s_server) rebuild_locked();
}

void hold(const std::string& sha256_hex, uint64_t size, const std::string& sd_path)
{
    const std::lock_guard<std::mutex> guard(s_lock);
    load_locked();
    s_book.hold({sha256_hex, size, sd_path});
    save_locked();
    if (s_server) rebuild_locked();
}

void release(const std::vector<std::string>& sd_paths)
{
    const std::lock_guard<std::mutex> guard(s_lock);
    load_locked();
    const size_t before = s_book.held().size();
    for (const std::string& p : sd_paths) s_book.release(p);
    if (s_book.held().size() == before) return;
    save_locked();
    if (s_server) rebuild_locked();
}

}   // namespace peer_flow
