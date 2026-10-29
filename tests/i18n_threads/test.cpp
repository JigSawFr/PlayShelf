// Host tests for Aurore's string tables (library/lib/core/i18n.cpp, with
// Aurore's patches 0001, 0012, 0018, 0019): a string looked up in the
// current language, else in English, else its name; another language loaded
// while the app runs; and strings read from other threads (the background
// tasks, the worker threads) while the UI thread loads another language,
// each read giving one language's string or the other's, never a table half
// replaced or freed under it (ASan would say so).
#include "check.h"
#include "temp_dir.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <aurore/core/application.hpp>
#include <aurore/core/i18n.hpp>
#include <aurore/core/util.hpp>
#include <string>
#include <thread>
#include <vector>

using namespace aurore::literals;

// What i18n.cpp needs of the rest of Aurore (util.cpp, its string and
// file helpers, is built with it).
namespace aurore
{
std::string Application::getLocale() { return LOCALE_FR; }
Point::Point() : x(0), y(0) {}
} // namespace aurore
// The theme colours the headers set up (never drawn here).
NVGcolor nvgRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return NVGcolor{{{r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f}}};
}

namespace
{
void write(const std::string& path, const char* text)
{
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream(path) << text;
}

void write_catalogs()
{
    write("i18n/en-US/main.json", R"({"hello": "Hello", "games": "{} games", "only_en": "English"})");
    write("i18n/fr/main.json", R"({"hello": "Bonjour", "games": "{} jeux"})");
    write("i18n/de/main.json", R"({"hello": "Hallo"})");
}

void test_lookups()
{
    aurore::loadTranslations();  // the platform's locale: French here
    CHECK(aurore::getStr("main/hello") == "Bonjour");
    CHECK(aurore::getStr("main/games", 3) == "3 jeux");
    CHECK("main/only_en"_i18n == "English");
    CHECK("main/missing"_i18n == "main/missing");

    aurore::loadTranslations("de");
    CHECK("main/hello"_i18n == "Hallo");
    CHECK(aurore::getStr("main/games", 2) == "2 games");

    aurore::loadTranslations(aurore::LOCALE_DEFAULT);
    CHECK("main/hello"_i18n == "Hello");

    aurore::loadTranslations("xx");  // no such folder: English
    CHECK("main/hello"_i18n == "Hello");
}

void test_reads_while_the_language_changes()
{
    std::atomic<bool> stop{false};
    std::atomic<int> wrong{0};
    std::atomic<long> reads{0};
    std::vector<std::thread> readers;
    for (int t = 0; t < 4; t++)
        readers.emplace_back([&] {
            while (!stop.load()) {
                std::string s = "main/hello"_i18n;
                std::string g = aurore::getStr("main/games", 5);
                if (s != "Hello" && s != "Bonjour" && s != "Hallo") wrong++;
                if (g != "5 games" && g != "5 jeux") wrong++;
                reads++;
            }
        });
    const char* langs[] = {"fr", "de", "en-US"};
    for (int i = 0; i < 300; i++) aurore::loadTranslations(langs[i % 3]);
    // Let the readers go on a while with the last table too.
    while (reads.load() < 1000) std::this_thread::yield();
    stop = true;
    for (std::thread& t : readers) t.join();
    CHECK(wrong.load() == 0);
    CHECK("main/hello"_i18n == "Hello");
}
} // namespace

// A sanitizer's report says where it was reached (as CI shows it).
extern "C" const char* __ubsan_default_options() { return "print_stacktrace=1"; }

int main()
{
    TempDir tmp("i18n_threads", TempDir::Enter);
    write_catalogs();
    test_lookups();
    test_reads_while_the_language_changes();
    return CHECK_DONE("i18n_threads tests passed");
}
