// homebrew/install.cpp: a single file put at its place, an archive unpacked
// and checked against its manifest, an update that drops a file, consent
// asked for the paths outside the app's own folders, a package that is not
// the announced one refused, the book written and read (schema, a newer one
// left alone), an uninstall that leaves no empty folder.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include <ctime>
#include <string>
#include <vector>

#include "check.h"
#include "fs.hpp"
#include "homebrew/install.hpp"
#include "temp_dir.hpp"
#include "util/sha256.hpp"
#include "util/zip_write.hpp"

using namespace homebrew;

namespace
{
lantern::Digest digest_of(const std::string& bytes)
{
    Sha256 h;
    h.update(bytes);
    return h.digest();
}

lantern::Homebrew entry_for(const std::string& id, const std::string& version, const std::string& format, const std::string& file_bytes,
                            const std::string& path)
{
    lantern::Homebrew e;
    e.id = id.c_str();
    e.platform = "switch";
    e.kind = "app";
    e.name = "Demo";
    e.version = version.c_str();
    e.build = 1;
    e.package.format = format.c_str();
    e.package.file = "/files/demo";
    e.package.size = file_bytes.size();
    e.package.sha256 = digest_of(file_bytes);
    e.package.path = path.c_str();
    lantern::ManifestEntry m;
    m.path = path.c_str();
    m.size = file_bytes.size();
    m.sha256 = e.package.sha256;
    e.package.manifest.push_back(m);
    e.package.manifest_sha256 = lantern::manifest_digest(e.package.manifest);
    return e;
}

void test_single_file()
{
    TempDir     t("hb_single");
    const std::string nro = "NRO0demo-bytes";
    const std::string pkg = t.path() + "/dl.nro", sd = t.path() + "/sd";
    fs::write(pkg, nro);
    const auto e = entry_for("org.example.demo", "1.0", "nro", nro, "/switch/org.example.demo/org.example.demo.nro");
    const auto out = install(e, pkg, sd, t.path() + "/stage", nullptr, nullptr);
    if (!CHECK(out.ok)) {
        fprintf(stderr, "  %s\n", out.why.c_str());
        return;
    }
    CHECK(fs::read(sd + "/switch/org.example.demo/org.example.demo.nro") == nro);
    CHECK(out.record.id == "org.example.demo" && out.record.files == std::vector<std::string>({ "/switch/org.example.demo/org.example.demo.nro" }));
    // A package that is not the announced one is refused and nothing written.
    fs::write(pkg, "other");
    const std::string sd2 = t.path() + "/sd2";
    CHECK(!install(e, pkg, sd2, t.path() + "/stage", nullptr, nullptr).ok && !fs::exists(sd2 + "/switch/org.example.demo/org.example.demo.nro"));
    // An uninstall takes the file and the folder.
    CHECK(uninstall(out.record, sd).size() == 1 && !fs::exists(sd + "/switch/org.example.demo"));
}

lantern::Homebrew zip_entry(const std::string& version, const std::vector<std::pair<std::string, std::string>>& files, const std::string& zip)
{
    lantern::Homebrew e = entry_for("org.example.demo", version, "zip", zip, "");
    e.package.manifest.clear();
    for (const auto& [path, bytes] : files) {
        lantern::ManifestEntry m;
        m.path = path.c_str();
        m.size = bytes.size();
        m.sha256 = digest_of(bytes);
        e.package.manifest.push_back(m);
    }
    e.package.manifest_sha256 = lantern::manifest_digest(e.package.manifest);
    return e;
}

std::string zip_of(const std::vector<std::pair<std::string, std::string>>& files)
{
    std::vector<std::pair<std::string, std::string>> named;
    for (const auto& [path, bytes] : files) named.push_back({ path.substr(1), bytes });
    const std::time_t now = 1700000000;
    std::tm tm {};
    gmtime_r(&now, &tm);
    return zip_write::stored(tm, named);
}

void test_archive()
{
    TempDir     t("hb_zip");
    const std::string sd = t.path() + "/sd", stage = t.path() + "/stage", pkg = t.path() + "/dl.zip";
    const std::string base = "/switch/org.example.demo/";
    const std::vector<std::pair<std::string, std::string>> v1 = { { base + "demo.nro", "one" }, { base + "data/a.bin", "aaa" }, { base + "old.txt", "old" } };
    const std::string z1 = zip_of(v1);
    fs::write(pkg, z1);
    const auto first = install(zip_entry("1.0", v1, z1), pkg, sd, stage, nullptr, nullptr);
    if (!CHECK(first.ok)) {
        fprintf(stderr, "  %s\n", first.why.c_str());
        return;
    }
    CHECK(fs::read(sd + base + "data/a.bin") == "aaa" && first.record.files.size() == 3);
    // The update ships two files only: old.txt goes.
    const std::vector<std::pair<std::string, std::string>> v2 = { { base + "demo.nro", "two" }, { base + "data/a.bin", "aaa" } };
    const std::string z2 = zip_of(v2);
    fs::write(pkg, z2);
    const auto second = install(zip_entry("2.0", v2, z2), pkg, sd, stage, &first.record, nullptr);
    CHECK(second.ok && second.removed == std::vector<std::string>({ base + "old.txt" }));
    CHECK(fs::read(sd + base + "demo.nro") == "two" && !fs::exists(sd + base + "old.txt"));
    // An archive with a file its manifest does not list is refused, nothing moved.
    const std::string z3 = zip_of({ { base + "demo.nro", "three" }, { base + "extra", "x" } });
    fs::write(pkg, z3);
    CHECK(!install(zip_entry("3.0", { { base + "demo.nro", "three" } }, z3), pkg, sd, stage, nullptr, nullptr).ok);
    CHECK(fs::read(sd + base + "demo.nro") == "two");
    // A path outside the app's folders asks the user; a refusal writes nothing.
    const std::vector<std::pair<std::string, std::string>> v4 = { { base + "demo.nro", "four" }, { "/atmosphere/contents/x/exefs.nsp", "p" } };
    const std::string z4 = zip_of(v4);
    fs::write(pkg, z4);
    size_t asked = 0;
    const auto no = install(zip_entry("4.0", v4, z4), pkg, sd, stage, nullptr, [&](const std::vector<std::string>& p) { asked = p.size(); return false; });
    CHECK(!no.ok && no.declined && asked == 1 && fs::read(sd + base + "demo.nro") == "two");
    CHECK(install(zip_entry("4.0", v4, z4), pkg, sd, stage, nullptr, [](const std::vector<std::string>&) { return true; }).ok);
    CHECK(fs::read(sd + "/atmosphere/contents/x/exefs.nsp") == "p");
}

void test_book()
{
    TempDir     t("hb_book");
    const std::string file = t.path() + "/homebrew.json";
    Book        b;
    CHECK(b.load(file) && b.all().empty());
    Record r;
    r.id = "demo";
    r.version = "1.0";
    r.platform = "switch";
    r.build = 3;
    r.files = { "/switch/demo/demo.nro" };
    b.put(r);
    CHECK(b.save(file));
    Book c;
    CHECK(c.load(file) && c.find("demo") && c.find("demo")->build == 3 && c.find("demo")->files == r.files);
    // A later schema is read as empty and never written over.
    fs::write(file, "{\"schema\":9,\"installed\":[{\"id\":\"x\"}]}");
    Book d;
    CHECK(d.load(file) && d.newer && d.all().empty() && !d.save(file));
    CHECK(fs::read(file).find("\"schema\":9") != std::string::npos);
    // Not JSON: not read.
    fs::write(file, "nope");
    CHECK(!Book().load(file));
}
}   // namespace

int main()
{
    test_single_file();
    test_archive();
    test_book();
    CHECK_DONE("homebrew install: a single file, the book, uninstall, a wrong package refused");
}
