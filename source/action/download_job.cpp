// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/download_job.hpp"

#include <algorithm>
#include <aurore.hpp>
#include <chrono>
#include <sys/stat.h>

#include "core/sim_env.h"
#include "download/recovery.hpp"
#include "install/console_core.hpp"
#include "install/prefix_source.hpp"
#include "lantern_app/package.hpp"
#include "lantern_app/range_source.hpp"
#include "ui/ui.hpp"
#include "util/config.hpp"
#include "util/cpu_boost.hpp"
#include "util/crypto_backend.hpp"

namespace download_job
{
uint64_t file_size(const std::string& path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 ? (uint64_t)st.st_size : 0;
}

namespace
{
// A package's source whose transfer, cut short (the network, a busy
// server), pauses the install instead of failing it: its journal kept, it
// is tried again from there. On the worker thread.
class CutSource : public install::Source
{
  public:
    CutSource(install::Source& src, Control& ctl, std::string* why) : inner(src), control(ctl), reason(why) {}
    uint64_t      size() const override { return inner.size(); }
    install::Done read(uint64_t offset, uint8_t* out, size_t n) override { return watch(inner.read(offset, out, n)); }
    install::Done stream(uint64_t offset, uint64_t length, const std::function<bool(const uint8_t*, size_t)>& sink) override
    {
        return watch(inner.stream(offset, length, sink));
    }

  private:
    install::Done watch(install::Done done)
    {
        if (done || control.stop) return done;
        const download::Cause c = download::cause_of(done.error());
        if (c == download::Cause::Network || c == download::Cause::Server) {
            *reason = done.error();
            control.cut = true;
            control.pause = true;
        }
        return done;
    }
    install::Source& inner;
    Control&         control;
    std::string*     reason;
};

// A package that is not on the SD card: the install reads its file only.
class LocalOnly : public install::Source
{
  public:
    explicit LocalOnly(uint64_t size) : total(size) {}
    uint64_t size() const override { return total; }
    install::Done read(uint64_t, uint8_t*, size_t) override { return std::unexpected("the package is not on the SD card"); }
    install::Done stream(uint64_t, uint64_t, const std::function<bool(const uint8_t*, size_t)>&) override
    {
        return std::unexpected("the package is not on the SD card");
    }

  private:
    uint64_t total;
};
}   // namespace

// On the worker thread.
Outcome run(const download::Job& job, const library::Server& server, const std::vector<download::Job>& snapshot,
            const std::shared_ptr<Control>& ctl, const std::string& fetched, const Hooks& hooks)
{
    using namespace install;
    Outcome o;
    auto    stopped = [&]() {
        o.paused = ctl->pause.load() && !ctl->cancel.load();
        o.cancelled = ctl->cancel.load();
        return o;
    };
    std::string cut_why;
    // What a fetch ahead left on the SD card (checked by the Lantern client as
    // it was written) is read from the file; the rest of the package is read
    // from the server where the installer asks, a signed range at a time.
    const std::string& path = fetched;
    if (job.size == 0 || job.sha256.empty()) {
        o.why = "the catalogue gave no size or checksum for this package: read the server again";
        return o;
    }
    const uint64_t        have = std::min(file_size(path), job.size);
    lantern_app::Platform platform;
    std::unique_ptr<lantern_app::RangeSource> remote;
    if (have < job.size) {
        std::string why;
        auto        client = lantern_app::open_client(platform, server, job.url, &why);
        if (!client) {
            o.why = why;
            return ctl->stop ? stopped() : o;
        }
        remote = std::make_unique<lantern_app::RangeSource>(std::move(client), lantern_app::PackageRequest{job.url, job.size, job.sha256, {}});
    }
    LocalOnly    local(job.size);
    PrefixSource prefixed(path, have, remote ? static_cast<install::Source&>(*remote) : local);
    CutSource    src(prefixed, *ctl, &cut_why);
    const auto pkg = package_open(src);
    if (!pkg) {
        o.why = pkg.error();
        return ctl->stop ? stopped() : o;
    }
    CoreConsole console(CoreStorage_Sd);
    if (!console.ok()) {
        o.why = console.error();
        return o;
    }
    // What an install cut short and given up (or another installer) left.
    for (const ContentId& id : download::orphans(console.placeholders(), snapshot)) {
        aurore::Logger::info("downloads: an orphan placeholder deleted ({})", content_id_hex(id));
        console.delete_placeholder(id);
    }
    const auto keys = core_keys();
    if (!keys) {
        o.why = keys.error();
        return o;
    }

    auto              last = std::chrono::steady_clock::now();
    InstallCallbacks  cb;
    cb.cancel = &ctl->cancel;
    cb.pause = &ctl->pause;
    cb.progress = [&](uint64_t done, uint64_t total, const std::string&) {
        // PlayShelf quitting: the job pauses, to go on at the next start.
        if (ui::quitting()) {
            ctl->pause = true;
            ctl->stop = true;
        }
        const auto t = std::chrono::steady_clock::now();
        if (t - last < std::chrono::milliseconds(500) && done < total) return;
        last = t;
        aurore::sync([hooks, done, total]() { hooks.progress(done, total); });
    };
    cb.journal = [hooks](const Journal& jn) {
        aurore::sync([hooks, jn]() { hooks.journal(jn); });
    };
    // The simulators' packages are signed with the host tests' key: with
    // PLAYSHELF_SIM_SIGNED (the web simulator's demo server), taken as
    // Nintendo's. Never set on the console.
    const bool unsigned_ok = job.accept_unsigned || sim_env_flag("PLAYSHELF_SIM_SIGNED");
    Installer  inst(src, *pkg, console, *keys,
                    InstallOptions{unsigned_ok ? SignaturePolicy::Off : SignaturePolicy::Require, false}, cb);
    if (job.journal) inst.resume_from(*job.journal);
    if (const Done prepared = inst.prepare(); !prepared) {
        o.why = prepared.error();
        return ctl->stop ? stopped() : o;
    }
    o.total = inst.bytes_to_write();
    const auto  began = std::chrono::steady_clock::now();
    cpu_boost::Guard boost(config::get().cpu_boost);
    const Done ran = inst.run();
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    // The pace of the install, with what it ran on (Developer › Encryption), to compare settings.
    if (ran.has_value() && seconds > 0.5)
        aurore::Logger::info("downloads: {} written: {} bytes in {:.1f} s, {:.1f} MB/s; {}", job.name, o.total, seconds,
                             (double)o.total / seconds / 1e6, crypto::status());
    o.ok = ran.has_value();
    if (!ran) o.why = ran.error();
    if (o.ok) o.programs = inst.programs();
    // The transfer cut, the user not pausing: a failure to try again, the
    // journal kept.
    if (!o.ok && inst.paused() && ctl->cut && !ctl->stop) {
        o.why = cut_why;
        return o;
    }
    if (!o.ok && (inst.paused() || ctl->cancel)) return stopped();
    o.undone = !o.ok;
    o.unsigned_content = !o.ok && o.why.contains("not signed by Nintendo");
    o.compressed_content = o.unsigned_content && inst.stopped_at_compressed();
    return o;
}

}   // namespace download_job
