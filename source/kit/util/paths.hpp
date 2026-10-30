// paths — where the app keeps its files, plus small filesystem helpers.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>
#include <vector>

namespace paths
{

// sd:/config/<app> (APP_ID, app_id.h) on the console (the .nro stays in
// sd:/switch/<app>); ./<app>_data on desktop. The folders only one app
// has are declared by the app, in this namespace, beside these.
std::string data_dir();
std::string logs_dir();
// Inside logs_dir() (util/log_files.hpp): the saved diagnostic reports, one
// file per crash, one per run (everything logged), one per report sent
// online.
std::string reports_dir();
std::string crashes_dir();
std::string sessions_dir();
std::string uploads_dir();
std::string config_file();
// The user's theme files (*.theme) and theme folders (<id>/theme.theme with
// its pictures), listed by the palette picker (Aurore's Theme::listThemes):
// none unless the user made the folder; the app never writes there.
std::string themes_dir();
// The last migration done and the build that last started (util/migrations.hpp).
std::string migrations_file();
// What can be fetched or read again (data_cleanup::cache empties it): the
// whole console's (lists fetched, icons) and, in nand_dir(), the system's
// own.
std::string cache_dir();
std::string nand_cache_dir();

// The folder of the system the app runs on (util/nand.hpp):
// data_dir()/nand/<key>. set_nand() once at start-up, before config::load();
// "unknown" until then.
void        set_nand(const std::string& key);
std::string nand_key();
std::string nand_dir();
std::string history_file();   // the change history (util/history.hpp), in nand_dir()

// Root of the SD card: "/" on the console; ./<app>_data/sd on desktop,
// so the simulated build can be given fake Atmosphère / sys-patch files.
std::string sd_root();

// mkdir -p. Returns false (errno preserved) on failure.
bool ensure_dir(const std::string& dir);

// Writes `content` to `path` through a ".tmp" sibling + rename, so a crash or a
// full SD card never leaves a half-written file behind (the content is
// written as "<path>.part" first: a ".tmp" there is always whole). On failure returns
// false and puts a short English reason in *error (when non-null).
[[nodiscard]] bool atomic_write(const std::string& path, const std::string& content, std::string* error = nullptr);

// Whole file. When `path` is missing but `path`.tmp exists (atomic_write
// stopped between its remove and its rename), reads that one instead.
bool read_file(const std::string& path, std::string& out);

// Part of a large file, for when only its start or its end is used: its
// first `head` bytes followed by its last `tail` bytes, the ones in between
// not read at all; the whole file when it holds no more than head + tail.
// Falls back to the ".tmp" as read_file does.
bool read_ends(const std::string& path, size_t head, size_t tail, std::string& out);
// A regular file at `path` itself (its ".tmp" not counted).
bool file_exists(const std::string& path);

// Whether `path` or its ".tmp" (see read_file) is there at all: one that is
// there but cannot be read is damaged, which is not the same as none.
bool exists_or_pending(const std::string& path);

// Replaces `target` with `fresh` (both on the same file system), for a
// file too large for atomic_write (a .nro). FAT has no atomic replace:
// target moves aside to "<target>.old", fresh takes its name, then the old
// one is removed, or kept as `keep_as` when given (it may be `fresh`'s own
// name: then the two swap). When fresh cannot take its place, the old one is
// put back. A missing target is simply created; with no target, `keep_as`
// is left as it is. Begins with recover_replace().
bool replace_file(const std::string& target, const std::string& fresh, const std::string& keep_as,
                  std::string* error);

// Finishes or undoes a replace_file() stopped half-way (a power cut between
// its renames): `target` missing and "<target>.old" there, that one is put
// back; both there, the old one becomes `keep_as` (or goes, without one).
void recover_replace(const std::string& target, const std::string& keep_as);

// Moves a damaged `path` (or its ".tmp", when only that one is left) to
// `path`.bad, so no later write replaces it. A `path`.bad already there
// becomes `path`.bad.1 (an older .bad.1 is dropped): the two most recent
// damaged copies are kept. False when it could not be moved.
bool put_aside(const std::string& path);

// Names (not paths) of regular files in `dir` ending with `suffix`, sorted.
std::vector<std::string> list_files(const std::string& dir, const std::string& suffix);
// The folders in `dir` (not "." or ".."), by name.
std::vector<std::string> list_dirs(const std::string& dir);

}   // namespace paths
