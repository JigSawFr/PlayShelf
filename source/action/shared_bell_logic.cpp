// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/shared_bell_logic.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <sys/stat.h>

#include "bl_app.h"
#include "bl_notices.h"

namespace shared_bell_logic
{

namespace
{
bool read_file(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

std::string under(const std::string& sd_root, const std::string& path)
{
    return sd_root + (sd_root.empty() || sd_root.back() != '/' ? "/" : "") + path;
}

// `s` into `out`, cut before a character that does not fit whole.
void copy(char* out, size_t cap, const std::string& s)
{
    size_t n = s.size() < cap ? s.size() : cap - 1;
    while (n > 0 && n < s.size() && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
    std::memcpy(out, s.data(), n);
    out[n] = '\0';
}
}   // namespace

std::string app_file(const std::string& sd_root)
{
    char path[BL_APP_PATH_MAX];
    bl_app_path(MODULE, path, sizeof(path));
    return under(sd_root, path);
}

std::string app_text(const std::string& version, const std::string& nro, const std::string& previous)
{
    BlApp a;
    bl_app_defaults(&a);
    if (!previous.empty()) bl_app_parse(&a, previous.data(), previous.size());
    if (!bl_app_may_write(&a)) return "";
    copy(a.name, sizeof(a.name), "PlayShelf");
    copy(a.version, sizeof(a.version), version);
    copy(a.repo, sizeof(a.repo), "JigSawFr/PlayShelf");
    const std::string under_root = !nro.empty() && nro[0] == '/' ? nro.substr(1) : nro;
    if (!under_root.empty() && under_root.size() < sizeof(a.nro)) copy(a.nro, sizeof(a.nro), under_root);
    std::string text(2048, '\0');
    const size_t n = bl_app_write(&a, &text[0], text.size());
    if (n == 0) return "";
    text.resize(n);
    return text == previous ? "" : text;
}

std::string route_of(const std::string& id)
{
    const auto starts = [&id](const char* p) { return id.rfind(p, 0) == 0; };
    if (id == "updates" || id == "dlcs" || id == "missing_updates") return "library";
    if (id == "new_games" || starts("wish.") || starts("sale.") || starts("out.") || starts("lowest.")) return "catalogue";
    if (starts("downloads_")) return "downloads";
    if (starts("kit.")) return "about";
    return "home";
}

std::string file(const std::string& sd_root)
{
    char path[BL_APP_PATH_MAX];
    bl_notices_path(MODULE, path, sizeof(path));
    return under(sd_root, path);
}

std::string text(const std::vector<Entry>& entries, const std::string& lang, const std::string& nro,
                 const std::string& previous, int64_t now)
{
    // Some 12 KB each, and as many tokens: on the heap, not the stack.
    auto before = std::make_unique<BlNotices>();
    auto after = std::make_unique<BlNotices>();
    auto toks = std::make_unique<BlJTok[]>(BL_NOTICES_TOKENS);
    std::string self = !nro.empty() && nro[0] == '/' ? nro.substr(1) : nro;
    bl_notices_init(before.get(), MODULE, "");
    if (!previous.empty() && bl_notices_parse(before.get(), previous.data(), previous.size(), toks.get(), BL_NOTICES_TOKENS)) {
        if (!bl_notices_may_write(before.get())) return "";
        if (self.empty()) self = before->nro;
    }
    bl_notices_init(after.get(), MODULE, self.c_str());
    std::memcpy(after->kept, before->kept, sizeof(after->kept));
    for (const Entry& e : entries) {
        if (after->count >= BL_NOTICES_MAX) break;
        BlNotice n = {};
        copy(n.id, sizeof(n.id), e.id);
        if (!n.id[0] || bl_notices_find(after.get(), n.id)) continue;
        n.level = e.todo ? BlNotice_Todo : BlNotice_Info;
        copy(n.title, sizeof(n.title), e.title);
        copy(n.detail, sizeof(n.detail), e.detail);
        copy(n.lang, sizeof(n.lang), lang);
        n.progress = -1;
        if (bl_route_valid(e.route.c_str())) copy(n.route, sizeof(n.route), e.route);
        n.updated = now;
        // Unchanged: the same `updated`, and what it did not know kept.
        if (const BlNotice* old = bl_notices_find(before.get(), n.id)) {
            std::memcpy(n.kept, old->kept, sizeof(n.kept));
            if (old->level == n.level && !std::strcmp(old->title, n.title) && !std::strcmp(old->detail, n.detail) &&
                !std::strcmp(old->lang, n.lang) && !std::strcmp(old->route, n.route) && old->progress == n.progress &&
                old->expires == n.expires)
                n.updated = old->updated;
        }
        bl_notices_put(after.get(), &n);
    }
    std::string out(64 * 1024, '\0');
    const size_t len = bl_notices_write(after.get(), &out[0], out.size());
    if (len == 0) return "";
    out.resize(len);
    return out == previous ? "" : out;
}

std::vector<Other> others(const std::string& sd_root, const std::string& lang, int64_t now)
{
    std::vector<Other> out;
    DIR* dir = opendir(under(sd_root, BL_NOTICES_DIR).c_str());
    if (!dir) return out;
    auto file_notices = std::make_unique<BlNotices>();
    auto toks = std::make_unique<BlJTok[]>(BL_NOTICES_TOKENS);
    while (const dirent* ent = readdir(dir)) {
        char module[BL_APP_MODULE_MAX];
        if (!bl_notices_module_of(ent->d_name, module, sizeof(module)) || !std::strcmp(module, MODULE)) continue;
        std::string text;
        if (!read_file(under(sd_root, BL_NOTICES_DIR "/") + ent->d_name, text)) continue;
        bl_notices_init(file_notices.get(), module, "");
        if (!bl_notices_parse(file_notices.get(), text.data(), text.size(), toks.get(), BL_NOTICES_TOKENS)) continue;
        // Its .nro, from its file; gone (the homebrew deleted): nothing.
        struct stat st;
        if (!file_notices->nro[0] || stat(under(sd_root, file_notices->nro).c_str(), &st) != 0) continue;
        Other o;
        o.module = module;
        o.nro = std::string(file_notices->nro[0] == '/' ? "" : "/") + file_notices->nro;
        o.name = module;
        char app_path[BL_APP_PATH_MAX];
        std::string app_text;
        if (bl_app_path(module, app_path, sizeof(app_path)) && read_file(under(sd_root, app_path), app_text)) {
            BlApp app;
            bl_app_defaults(&app);
            bl_app_parse(&app, app_text.data(), app_text.size());
            if (app.name[0]) o.name = app.name;
        }
        for (size_t i = 0; i < file_notices->count; i++) {
            const BlNotice& n = file_notices->items[i];
            if (!bl_notice_shown(&n, now, lang.c_str())) continue;
            Notice shown;
            shown.id = n.id;
            shown.attention = n.level == BlNotice_Todo || n.level == BlNotice_Error;
            shown.title = n.title;
            shown.detail = n.detail;
            if (n.progress >= 0)
                shown.detail += (shown.detail.empty() ? "" : " · ") + std::to_string(n.progress) + "\u00a0%";
            shown.route = n.route;
            o.notices.push_back(shown);
        }
        if (!o.notices.empty()) out.push_back(std::move(o));
    }
    closedir(dir);
    std::sort(out.begin(), out.end(), [](const Other& a, const Other& b) { return a.name < b.name; });
    return out;
}

}   // namespace shared_bell_logic
