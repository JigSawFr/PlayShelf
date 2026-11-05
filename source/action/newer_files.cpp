// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/newer_files.hpp"

#include <string>
#include <vector>

#include <aurore.hpp>

#include "action/notices.hpp"
#include "download/queue.hpp"
#include "library/facts.hpp"
#include "library/news.hpp"
#include "library/seen.hpp"
#include "library/servers.hpp"
#include "util/paths.hpp"

namespace newer_files
{

namespace
{
struct Reader
{
    const char* name;
    bool (*newer)(const std::string& text);
};

// Every file of the data folder a reader leaves alone when it is of a newer
// schema (facts.json is in nlib/, the others in the folder itself).
const Reader READERS[] = {
    { "servers.json", library::servers_newer },
    { "queue.json", download::queue_newer },
    { "seen.json", library::SeenBook::newer },
    { "news.json", library::TellBook::newer },
    { "rebuilt.txt", library::ids_newer },
    { "not_interested.txt", library::ids_newer },
    { "nlib/facts.json", library::FactsBook::newer },
};
}   // namespace

std::vector<std::string> list()
{
    std::vector<std::string> out;
    for (const Reader& r : READERS) {
        std::string text;
        if (paths::read_file(paths::data_dir() + "/" + r.name, text) && r.newer(text)) out.push_back(r.name);
    }
    return out;
}

void notify()
{
    const std::vector<std::string> names = list();
    if (names.empty()) {
        notices::clear("newer_files");
        return;
    }
    std::string joined;
    for (const std::string& n : names) joined += (joined.empty() ? "" : ", ") + n;
    notices::set("newer_files", notices::Level::Info, aurore::getStr("playshelf/notices/newer_files"),
                 aurore::getStr("playshelf/notices/newer_files_detail", joined));
}

}   // namespace newer_files
