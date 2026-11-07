// What a Lantern server holds for a game besides its packages, through the
// SDK's client: cheat sets (SPEC.md § 9), save backups (§ 10) and game
// requests (§ 11). Each call connects on its own (the address chosen, the
// key pinned at pairing, this console's token), so any thread may make it.
// Failures say why in `why`, for a person.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "library/servers.hpp"
#include "library/services.hpp"

namespace lantern_app
{

// The cheat sets of a game (0: every game's).
[[nodiscard]] bool list_cheats(const library::Server& server, uint64_t title_id, std::vector<library::Cheat>* out, std::string* why);
[[nodiscard]] bool fetch_cheat(const library::Server& server, const library::Cheat& cheat, std::string* text, std::string* why);

[[nodiscard]] bool list_saves(const library::Server& server, std::vector<library::SaveBackup>* out, std::string* why);
// `zip_path` sent as the backup of `title_id`'s account save; progress as (sent, total).
bool upload_save(const library::Server& server, uint64_t title_id, const std::string& zip_path, const std::string& note,
                 const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel, std::string* why);
// The archive of `backup` written to `zip_path`, whole and checked.
bool download_save(const library::Server& server, const library::SaveBackup& backup, const std::string& zip_path,
                   const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>* cancel, std::string* why);
[[nodiscard]] bool delete_save(const library::Server& server, const library::SaveBackup& backup, std::string* why);

[[nodiscard]] bool list_requests(const library::Server& server, std::vector<library::TitleRequest>* out, std::string* why);
// `message`: what the server says of the request, when it says something.
bool request_title(const library::Server& server, uint64_t title_id, std::string* message, std::string* why);

}   // namespace lantern_app
