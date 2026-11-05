// compress_title — an installed game made over with its program stored
// compressed (recompress.hpp), as a transaction that never touches what is
// there until the new copy has been written, registered and checked, and
// can be taken back until the user has seen the game run.
//
//   prepare()  nothing is written. The title is read (its record, its content
//              meta, its program), the RomFS it is made from is checked against
//              its own integrity tree, the program is made over once to measure
//              what it comes to, and the preconditions are checked: the
//              firmware reads the layer, the title is an application with no
//              update installed (or one to merge, CompressOptions::merge_update),
//              nothing sold with a ticket, room for the new copy beside the old
//              one, a gain that is worth it. The caller shows the numbers;
//              confirm() decides.
//   run()      the journal is saved first (the old record, the ids this will
//              make); the program is made over again into a placeholder and
//              registered under the id its hash gives (the second pass must
//              give the first one's bytes); the content meta is rebuilt the
//              same way; then everything new is read back: the program's
//              SHA-256, its header, its other sections, its integrity tree,
//              its table, and the RomFS it decompresses to, byte for byte
//              against the original's; the content meta's. Only then is the
//              record replaced (and committed), and read back. A failure, a
//              cancel or a full disk anywhere before that deletes what this
//              made and leaves the title as it was; one after it puts the old
//              record back.
//   keep()     the user ran the game: the old program and content meta are
//              deleted. This is the one step that cannot be taken back.
//   revert()   the old record is put back and the new contents deleted.
//   recover()  at start-up: a journal left by a run that did not end. It
//              deletes what a run that never replaced the record made, and
//              reports a run that did, for the user to keep or revert. When
//              unsure it keeps everything.
//
// The result is not signed by Nintendo: it runs only with signature patches,
// and no official update installs over it.
//
// Plain C++ over Console; the codec and the journal's storage are the caller's.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "install/installer.hpp"
#include "install/recompress.hpp"

namespace install
{

// What a run keeps, to undo it or finish it after a restart. Text, versioned
// (`schema`), read by the program that wrote it.
struct CompressJournal
{
    enum class Phase { Writing, Swapped, Keeping };   // Keeping: the user kept the result, the old contents are being deleted
    Phase                  phase = Phase::Writing;
    MetaKey                key;               // the game's record
    std::vector<uint8_t>   old_record, new_record;   // new_record: empty until it is known
    ContentId              old_program{}, old_meta{};
    // Merging an update: the new record's key (the update's version), and the
    // update's record and contents, which the merged game replaces.
    std::optional<MetaKey> new_key;
    std::optional<MetaKey> patch_key;
    std::vector<uint8_t>   patch_record;
    ContentId              old_patch_program{}, old_patch_meta{};
    MetaKey                result_key() const { return new_key ? *new_key : key; }
    std::vector<ContentId> placeholders;   // made by this run
    std::vector<ContentId> contents;       // registered by this run

    std::string                         text() const;
    static std::optional<CompressJournal> parse(const std::string& text);   // none for another schema
};

class JournalStore
{
  public:
    virtual ~JournalStore() = default;
    virtual std::optional<std::string> load() = 0;
    // False when it could not be kept durably: the run does not go on.
    [[nodiscard]] virtual bool save(const std::string& text) = 0;
    virtual void               clear() = 0;
};

struct CompressOptions
{
    uint32_t block_size = cstore::MAX_BLOCK;
    uint64_t free_margin = 256ull << 20;   // room left after the new copy
    uint32_t min_saving_percent = 5;       // below it, not worth losing official updates
    uint32_t min_system_version = 12u << 16;
    bool     update_installed = false;     // the caller knows: an update is installed (refused)
    // The update installed over the game, to merge into it (its key: the game's
    // id + 0x800, type Patch). The game then becomes one title at the update's
    // version, with the update's program, control data and the like, and its
    // two records are replaced by one. An official update cannot be installed
    // over it afterwards.
    std::optional<MetaKey> merge_update;
};

enum class CompressStage { Measuring, Writing, Checking };

struct CompressReport
{
    uint64_t program_before = 0, program_after = 0;   // the program NCA's bytes
    uint64_t free_space = 0, needed = 0;
    uint32_t saving_percent() const { return program_before ? (uint32_t)((program_before - std::min(program_before, program_after)) * 100 / program_before) : 0; }
};

struct CompressCallbacks
{
    std::function<void(CompressStage, uint64_t done, uint64_t total)> progress;
    const std::atomic<bool>*                                          cancel = nullptr;
    // After prepare()'s numbers: whether to go on. Not given: yes.
    std::function<bool(const CompressReport&)> confirm;
};

class TitleCompressor
{
  public:
    TitleCompressor(Console& console, NcaKeys keys, JournalStore& journal, cstore::CompressFn compress,
                    cstore::DecompressFn decompress, MetaKey key, CompressOptions options = {}, CompressCallbacks callbacks = {});
    ~TitleCompressor();

    [[nodiscard]] Done prepare();
    const CompressReport& report() const { return rep; }

    // Writes, checks, replaces the record. Why not when it failed (the title
    // is as it was).
    [[nodiscard]] Done run();
    // After a successful run(), once the user has seen the game run.
    [[nodiscard]] Done keep();
    [[nodiscard]] Done revert();

    // A journal left behind, handled as the header says; what it found.
    enum class Found { Nothing, CleanedUp, AwaitingDecision };
    struct Left
    {
        Found   found = Found::Nothing;
        MetaKey key;   // the title awaiting a decision
    };
    [[nodiscard]] static Parsed<Left> recover(Console& console, JournalStore& journal);
    // keep() or revert() for the title recover() reported.
    [[nodiscard]] static Done keep_left(Console& console, JournalStore& journal);
    [[nodiscard]] static Done revert_left(Console& console, JournalStore& journal);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    CompressReport        rep;
};

}   // namespace install
