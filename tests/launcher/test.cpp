// Host tests for source/util/launcher.cpp against a fake core/platform.h:
// which store is found on the SD card for each preference, and what reaches
// hbloader (an "sdmc:/" path, nothing when PlayShelf was not started by it),
// and the Homebrew App Store's record of PlayShelf.
#include "check.h"
#include "temp_dir.hpp"
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <unistd.h>

#include "core/platform.h"
#include "util/launcher.hpp"
#include "util/paths.hpp"

static bool        g_hbloader = false;
static std::string g_next;
static int         g_calls = 0;

extern "C" bool platform_can_launch(void) { return g_hbloader; }
extern "C" bool platform_set_next_load(const char* path)
{
    g_calls++;
    if (!g_hbloader) return false;
    g_next = path;
    return true;
}

static void install(const std::string& sd_path)
{
    const std::string full = paths::sd_root() + sd_path;
    CHECK(std::system(("mkdir -p '" + full.substr(0, full.rfind('/')) + "'").c_str()) == 0);
    // 0644, not fopen's 0666.
    const int fd = ::open(full.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    std::FILE* f = fd >= 0 ? ::fdopen(fd, "w") : nullptr;
    if (!CHECK(f)) return;
    std::fputs("NRO0", f);
    std::fclose(f);
}

int main()
{
    TempDir tmp("launcher", TempDir::Enter);   // paths::sd_root() is ./playshelf_data/sd on the host

    using launcher::Store;
    // Nothing installed: no store, whatever the preference.
    for (const char* pref : { "", "auto", "sphaira", "appstore", "manual", "inplace" }) CHECK(launcher::find(pref).store == Store::None);

    // A folder under a store's name is not the store.
    CHECK(std::system("mkdir -p playshelf_data/sd/switch/appstore/appstore.nro") == 0);
    CHECK(launcher::find("appstore").store == Store::None);
    CHECK(std::system("rmdir playshelf_data/sd/switch/appstore/appstore.nro") == 0);

    install("/switch/appstore/appstore.nro");
    CHECK(launcher::find("auto").store == Store::AppStore);
    CHECK(launcher::find("").store == Store::AppStore);          // not chosen yet: as auto
    CHECK(launcher::find("inplace").store == Store::None);       // PlayShelf itself, not a store
    CHECK(launcher::find("sphaira").store == Store::None);
    install("/switch/sphaira.nro");   // sphaira's second place
    CHECK(launcher::find("auto").path == "/switch/sphaira.nro");
    install("/switch/sphaira/sphaira.nro");
    const launcher::Target sphaira = launcher::find("auto");
    CHECK(sphaira.store == Store::Sphaira && sphaira.path == "/switch/sphaira/sphaira.nro");
    CHECK(launcher::find("appstore").path == "/switch/appstore/appstore.nro");
    CHECK(launcher::find("manual").store == Store::None);
    // Not started through hbloader: nothing to hand over.
    CHECK(!launcher::can_launch() && !launcher::launch(sphaira) && g_next.empty());
    g_hbloader = true;
    CHECK(launcher::can_launch());
    CHECK(launcher::launch(sphaira) && g_next == "sdmc:/switch/sphaira/sphaira.nro");
    const int calls = g_calls;
    CHECK(!launcher::launch(launcher::Target{}) && g_calls == calls);   // Store::None never reaches the loader
    CHECK(!launcher::launch(launcher::Target{ Store::Sphaira, "switch/x.nro" }) && g_calls == calls);
    // PlayShelf itself, after a development build replaced it.
    CHECK(launcher::launch_nro("/switch/playshelf/playshelf.nro") && g_next == "sdmc:/switch/playshelf/playshelf.nro");
    const int nro_calls = g_calls;
    CHECK(!launcher::launch_nro("") && !launcher::launch_nro("switch/playshelf.nro") && g_calls == nro_calls);

    // The App Store's record of PlayShelf (libget's): its manifest, not its info.json alone.
    const std::string record = "/switch/appstore/.get/packages/PlayShelf/";
    CHECK(!launcher::installed_by_store());
    install(record + "info.json");
    CHECK(!launcher::installed_by_store());
    install(record + "manifest.install");
    CHECK(launcher::installed_by_store());
    // Another package's record is not PlayShelf's.
    CHECK(std::system("rm -r playshelf_data/sd/switch/appstore/.get/packages/PlayShelf") == 0);
    install("/switch/appstore/.get/packages/PctlManager/manifest.install");
    CHECK(!launcher::installed_by_store());

    CHECK(tmp.remove());
    return CHECK_DONE("launcher store search, App Store record and hbloader hand-off assertions passed");
}
