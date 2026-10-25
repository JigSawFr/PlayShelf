// installer — one package installed as one transaction: everything is
// checked before anything is written (each content meta read, every
// content it needs found in the package or already installed, the space,
// the system version, a certificate for each ticket), then the contents
// are written to placeholders and registered, the tickets imported, the
// content meta records set and the application records pushed. Anything
// that fails on the way undoes what this install did: the placeholders and
// contents it made are deleted, the records it replaced put back.
//
// An install can also be paused (InstallCallbacks::pause): it stops at the
// next piece and keeps what it wrote, the contents registered and the one
// being written, as its journal says (InstallCallbacks::journal, given
// every JOURNAL_EVERY bytes and when it stops). Started again with that
// journal (Installer::resume_from), it skips the contents registered and
// takes a plain .nca up where its journal left it; a .ncz starts again.
// Cancelled or failed then, it undoes what every run of it did.
//
// The console is an interface (Console): ncm, its content meta database, ns
// and es on the Switch (core/), a folder on a PC (sim/), memory in the tests.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "install/cnmt.hpp"
#include "install/content_stream.hpp"
#include "install/nca.hpp"
#include "install/package.hpp"

namespace install
{

// A content meta record's key (NcmContentMetaKey).
struct MetaKey
{
    uint64_t id = 0;
    uint32_t version = 0;
    MetaType type = MetaType::Application;
    bool     operator==(const MetaKey& o) const { return id == o.id && version == o.version && type == o.type; }
};

class Console
{
  public:
    virtual ~Console() = default;
    // The content storage installed to (the SD card's, or the system's).
    // A placeholder is named after the content it becomes.
    virtual bool     has_content(const ContentId& id) = 0;
    virtual bool     create_placeholder(const ContentId& id, uint64_t size, std::string* why) = 0;
    virtual bool     write_placeholder(const ContentId& id, uint64_t offset, const uint8_t* data, size_t size,
                                       std::string* why) = 0;
    virtual bool     register_placeholder(const ContentId& id, std::string* why) = 0;
    virtual bool     has_placeholder(const ContentId& id) = 0;
    virtual void     delete_placeholder(const ContentId& id) = 0;
    // Every placeholder of the storage (an install's left behind by a
    // restart, or another installer's).
    virtual std::vector<ContentId> placeholders() = 0;
    virtual void     delete_content(const ContentId& id) = 0;
    virtual uint64_t free_space() = 0;
    // Its content meta database.
    virtual std::optional<std::vector<uint8_t>> get_meta(const MetaKey& key) = 0;
    virtual bool set_meta(const MetaKey& key, const std::vector<uint8_t>& record, std::string* why) = 0;
    virtual bool remove_meta(const MetaKey& key, std::string* why) = 0;
    virtual bool commit_meta(std::string* why) = 0;
    // ns and es.
    virtual bool push_application_record(uint64_t application_id, const MetaKey& key, std::string* why) = 0;
    virtual bool import_ticket(const std::vector<uint8_t>& ticket, const std::vector<uint8_t>& certificate,
                               std::string* why) = 0;
    // The firmware's version, as hosversionGet() gives it: major << 16 | minor << 8 | micro.
    virtual uint32_t system_version() = 0;
};

// What to do with an NCA whose header is not signed with Nintendo's key
// (or with a key the installer does not know): refuse it, ask, or take it.
enum class SignaturePolicy { Require, Ask, Off };

// What an install did so far, to go on with it: the contents it
// registered (a cancel deletes them), and the one it was writing with
// where its stream stood (a plain .nca only).
struct Journal
{
    std::vector<ContentId>                   registered;
    std::optional<ContentId>                 writing;
    std::optional<ContentStream::Checkpoint> at;
    bool operator==(const Journal& o) const;
};

constexpr uint64_t JOURNAL_EVERY = 32u << 20;

struct InstallOptions
{
    SignaturePolicy signatures = SignaturePolicy::Require;
    bool            ignore_required_system_version = false;
};

struct InstallCallbacks
{
    // The bytes written so far of those the install writes, and the
    // content being written ("<id>.nca").
    std::function<void(uint64_t done, uint64_t total, const std::string& content)> progress;
    // SignaturePolicy::Ask: whether to take this content anyway. Once yes,
    // the rest of the package is taken too.
    std::function<bool(const std::string& content)> accept_signature;
    // Set from another thread: the install stops at the next piece and
    // undoes what it did.
    const std::atomic<bool>* cancel = nullptr;
    // The same, keeping what it did (run() returns false, paused() true).
    const std::atomic<bool>* pause = nullptr;
    // Where the install stands, to keep for a later run (a file the
    // queue writes): every JOURNAL_EVERY bytes, and when it pauses.
    std::function<void(const Journal&)> journal;
};

// A title of the package, as prepare() found it.
struct TitlePlan
{
    ContentMeta              meta;
    ContentInfo              meta_content;   // the .cnmt.nca itself
    std::vector<uint8_t>     meta_nca;       // its bytes (an NCA, a .cnmt.ncz decompressed)
    std::vector<ContentInfo> to_write;       // its contents not installed yet (the .cnmt.nca's too)
};

class Installer
{
  public:
    Installer(Source& src, Package pkg, Console& console, NcaKeys keys, InstallOptions options = {},
              InstallCallbacks callbacks = {});

    // Reads the content metas and checks the package can be installed as
    // a whole; nothing is written. False with `why`: nothing to undo.
    bool prepare(std::string* why);
    // Before run(): goes on from an earlier run of the same package,
    // paused or cut short, as its journal says.
    void resume_from(const Journal& j) { resume = j; }
    const std::vector<TitlePlan>& plan() const { return titles; }
    uint64_t                      bytes_to_write() const { return total; }

    // Installs what prepare() planned. False with `why` when it failed or
    // was cancelled (what every run did is undone then), or was paused
    // (paused(): nothing undone, the journal given).
    bool run(std::string* why);
    bool paused() const { return was_paused; }
    // The bytes already written by earlier runs, of bytes_to_write().
    uint64_t resumed_bytes() const { return resumed; }

  private:
    bool write_content(const ContentInfo& c, std::string* why);
    bool write_meta_content(const TitlePlan& t, std::string* why);
    bool import_tickets(std::string* why);
    void undo();
    bool cancelled() const { return cb.cancel && cb.cancel->load(); }
    bool pausing() const { return cb.pause && cb.pause->load(); }
    void save_journal();

    Source&                src;
    Package                pkg;
    Console&               console;
    NcaKeys                keys;
    InstallOptions         options;
    InstallCallbacks       cb;
    std::vector<TitlePlan> titles;
    uint64_t               total = 0, done = 0, resumed = 0, journaled = 0;
    bool                   signatures_accepted = false;
    bool                   was_paused = false;
    Journal                state;   // what the journal says now
    std::optional<Journal> resume;

    // What run() did, to undo.
    struct MetaBackup
    {
        MetaKey                             key;
        std::optional<std::vector<uint8_t>> previous;
    };
    std::vector<ContentId>  made;   // placeholders created, contents registered
    std::vector<MetaBackup> metas;
};

}   // namespace install
