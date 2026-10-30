// The desktop build's console (sim/sim_install.cpp) behind install::Console
// (install/console_core.cpp), with the keys it derives: an NSZ built with the
// test keys installed in a folder, its contents, content meta record,
// application record and ticket where the simulated console keeps them; the
// same package again writes nothing; no room refused before writing; a
// placeholder left behind made anew; a failure after the contents undone. And result_text(), the console's error codes.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "../install/packages.hpp"
#include "fs.hpp"
#include "install/console_core.hpp"
#include "install/installer.hpp"
#include "temp_dir.hpp"

using namespace install;
using namespace test_install;

namespace
{
std::string file(const std::string& path)
{
    std::ifstream     in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

std::string bytes(const Bytes& b) { return std::string(b.begin(), b.end()); }

bool install_package(const Bytes& package, std::string* why)
{
    CoreConsole console(CoreStorage_Sd);
    if (!console.ok()) {
        *why = console.error();
        return false;
    }
    const auto keys = core_keys(why);
    if (!keys) return false;
    MemorySource src(package);
    const auto   pkg = package_open(src, why);
    if (!pkg) return false;
    Installer inst(src, *pkg, console, *keys, {SignaturePolicy::Off, false});
    return inst.prepare(why) && inst.run(why);
}

void test_installed()
{
    TempDir     tmp("install_sim", TempDir::Enter);
    const Title t = application(true);
    std::string why;
    if (!CHECK(install_package(nsp(t), &why))) {
        fprintf(stderr, "  %s\n", why.c_str());
        return;
    }
    const std::string root = "playshelf_data/sim_console";
    CHECK(file(root + "/contents/" + content_id_hex(t.program_info.id) + ".nca") == bytes(t.program));
    CHECK(file(root + "/contents/" + content_id_hex(t.control_info.id) + ".nca") == bytes(t.control));
    CHECK(file(root + "/contents/" + content_id_hex(t.meta_id) + ".nca") == bytes(t.meta_nca));
    const auto  meta = cnmt_parse(t.cnmt);
    ContentInfo mc = info_of(t.meta_nca, ContentType::Meta, 0x44);
    CHECK(file(root + "/meta/0100000000010000-0.bin") == bytes(cnmt_install_record(*meta, mc)));
    CHECK(file(root + "/records.log") == "0100000000010000 0100000000010000-0 type=0x80\n");
    CHECK(file(root + "/tickets/0.tik").size() == 0x2c0 && file(root + "/tickets/0.cert").size() == 0x700);
    CHECK(!fs::exists(root + "/placeholders/" + content_id_hex(t.program_info.id)));

    // Everything there: nothing written again, the record pushed again.
    CHECK(install_package(nsp(t), &why));
    CHECK(file(root + "/records.log").size() == 2 * 46);
}

void test_refusals()
{
    TempDir     tmp("install_sim", TempDir::Enter);
    const Title t = application();
    std::string why;
    setenv("PLAYSHELF_SIM_FREE_MB", "0", 1);
    CHECK(!install_package(nsp(t), &why) && why.find("space") != std::string::npos);
    unsetenv("PLAYSHELF_SIM_FREE_MB");
    CHECK(!fs::exists("playshelf_data/sim_console/meta/0100000000010000-0.bin"));

    // A placeholder an earlier attempt left behind: deleted, made anew.
    fs::make_dirs("playshelf_data/sim_console/placeholders");
    fs::write("playshelf_data/sim_console/placeholders/" + content_id_hex(t.control_info.id), "x");
    CHECK(install_package(nsp(t), &why));
    CHECK(file("playshelf_data/sim_console/contents/" + content_id_hex(t.control_info.id) + ".nca") == bytes(t.control));
}

void test_undo()
{
    TempDir     tmp("install_sim", TempDir::Enter);
    const Title t = application();
    std::string why;
    // The tickets' folder a file: the ticket cannot be imported, after the
    // contents were written. They go again, no record is left.
    fs::make_dirs("playshelf_data/sim_console");
    fs::write("playshelf_data/sim_console/tickets", "not a folder");
    CHECK(!install_package(nsp(t), &why) && why.find("ticket") != std::string::npos);
    for (const ContentInfo* c : {&t.program_info, &t.control_info})
        CHECK(!fs::exists("playshelf_data/sim_console/contents/" + content_id_hex(c->id) + ".nca"));
    CHECK(!fs::exists("playshelf_data/sim_console/contents/" + content_id_hex(t.meta_id) + ".nca"));
    CHECK(!fs::exists("playshelf_data/sim_console/meta/0100000000010000-0.bin"));
    CHECK(!fs::exists("playshelf_data/sim_console/records.log"));
}

void test_result_text()
{
    // Module 400, description 80: the simulated console's failure.
    CHECK(result_text(NXM_RESULT(80), "writing") == "writing: 2400-0080");
    CHECK(result_text(0x2a07, "x") == "x: 2007-0021");   // module 7 (fs-like), description 21
}
}   // namespace

int main()
{
    test_installed();
    test_refusals();
    test_undo();
    test_result_text();
    CHECK_DONE("install on the simulated console: an NSZ installed in a folder, refusals and undo, error codes passed");
}
