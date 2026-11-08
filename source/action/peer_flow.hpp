// peer_flow — this console serving its files to the other consoles of its
// network (SPEC.md 19.4), once its user turns it on (config share_files, off
// by default). It keeps the peers book (config/playshelf/peers.json): the
// grant key of each server the console paired with, and the homebrew files
// it holds whole; while the setting is on, it runs lantern_app::PeerServer
// over them, and while it is off nothing listens.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <cstdint>
#include <string>
#include <vector>

namespace peer_flow
{

// At start-up, and when the setting changes: the server runs exactly while
// config share_files is on.
void apply();
void shutdown();

// The grant key (64 hex digits) a paired server's handshake gave.
void note_server(const std::string& server_uid, const std::string& key_hex);

// A file written to the SD card whole, whose size and SHA-256 were checked
// against the server's document; `sd_path` starts at the SD root with '/'.
void hold(const std::string& sha256_hex, uint64_t size, const std::string& sd_path);
// How many files this console holds whole for the others (what it gives
// while sharing is on).
size_t held_count();

// How many other consoles answer on this network for the first paired
// server (asked in the background, at most once a minute); `done` runs on the
// UI thread, 0 when there is no server or no answer.
void count_nearby(std::function<void(size_t)> done);

// Files that are gone or changed.
void release(const std::vector<std::string>& sd_paths);

}   // namespace peer_flow
