// A fresh folder under /tmp for a suite that writes files, removed with the
// object: TempDir tmp("config") makes /tmp/playshelf_config_XXXXXX;
// TempDir tmp("config", TempDir::Enter) also makes it the working directory
// (paths::data_dir() is ./playshelf_data on the host) until it is removed,
// when the previous one comes back. CHECK(tmp.remove()) where a suite checks
// that the folder went; the destructor removes it otherwise.
#ifndef PLAYSHELF_TESTS_TEMP_DIR_HPP
#define PLAYSHELF_TESTS_TEMP_DIR_HPP

#include <climits>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "check.h"

class TempDir
{
  public:
    enum Cwd { Stay, Enter };

    explicit TempDir(const char* name, Cwd cwd = Stay)
    {
        std::string tmpl = std::string("/tmp/playshelf_") + name + "_XXXXXX";
        REQUIRE(mkdtemp(tmpl.data()) != nullptr);
        path_ = tmpl;
        if (cwd == Enter) {
            char here[PATH_MAX];
            REQUIRE(getcwd(here, sizeof here) != nullptr);
            back_ = here;
            REQUIRE(chdir(path_.c_str()) == 0);
        }
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    ~TempDir() { remove(); }

    const std::string& path() const { return path_; }

    // Back to the previous working directory (when entered), then the folder
    // and everything in it removed: true when both went well. Once removed,
    // a no-op.
    bool remove()
    {
        if (path_.empty()) return true;
        bool ok = true;
        if (!back_.empty()) ok = chdir(back_.c_str()) == 0;
        ok = std::system(("rm -rf '" + path_ + "'").c_str()) == 0 && ok;
        path_.clear();
        back_.clear();
        return ok;
    }

  private:
    std::string path_;
    std::string back_;
};

#endif
