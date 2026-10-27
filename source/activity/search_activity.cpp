// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/search_activity.hpp"

#include <map>
#include <set>
#include <vector>

#include <borealis/cadran/ui/cadran_kit.hpp>
#include <borealis/cadran/ui/cadran_list.hpp>

#include "action/game_page.hpp"
#include "action/insights.hpp"
#include "action/installed_flow.hpp"
#include "action/library_flow.hpp"
#include "action/nlib_flow.hpp"
#include "library/servers.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;
namespace kit = cadran::kit;

namespace
{
// Every word of `query` (folded) in `text` (folded).
bool has_words(const std::string& text, const std::vector<std::string>& words)
{
    for (const std::string& w : words)
        if (text.find(w) == std::string::npos) return false;
    return !words.empty();
}

std::vector<std::string> words_of(const std::string& query)
{
    std::vector<std::string> out;
    const std::string        folded = library::fold(query);
    size_t                   start = 0;
    while (start < folded.size()) {
        size_t end = folded.find(' ', start);
        if (end == std::string::npos) end = folded.size();
        if (end > start) out.push_back(folded.substr(start, end - start));
        start = end + 1;
    }
    return out;
}
}   // namespace

SearchActivity::SearchActivity(std::string q)
    : query(std::move(q))
{
}

void SearchActivity::ask(const std::string& initial)
{
    ui::prompt_text("playshelf/home/search_header"_i18n, initial, 64, [](std::string q) {
        if (words_of(q).empty()) return;
        brls::Application::pushActivity(new SearchActivity(q));
    });
}

void SearchActivity::onContentAvailable()
{
    if (auto* frame = dynamic_cast<brls::AppletFrame*>(this->getContentView()))
        frame->setTitle(brls::getStr("playshelf/home/search_title", this->query));
    root->setPadding(10, 72, 32, 72);
    // Y: other words, in place of these.
    this->getContentView()->registerAction("playshelf/library/search"_i18n, brls::BUTTON_Y, [this](brls::View*) {
        const std::string was = this->query;
        brls::Application::popActivity(brls::TransitionAnimation::NONE, [was]() { SearchActivity::ask(was); });
        return true;
    });
    this->rebuild();
}

void SearchActivity::rebuild()
{
    const std::vector<std::string> words = words_of(this->query);
    const auto                     installed = library_flow::installed();

    // Each game once: its first server's catalogue, and the servers that have it.
    struct Hit
    {
        insights::Pick           pick;
        std::vector<std::string> servers;
    };
    std::vector<Hit>              hits;
    std::map<uint64_t, size_t>    at;
    for (const library::Server& s : library::load_servers()) {
        const auto l = library_flow::loaded(s.id);
        if (!l) continue;
        for (size_t i = 0; i < l->titles.size() && hits.size() < MAX_RESULTS; i++) {
            const library::Title& t = l->titles[i];
            const auto            it = t.id ? at.find(t.id) : at.end();
            if (it != at.end()) {
                hits[it->second].servers.push_back(s.name);
                continue;
            }
            if (!has_words(library::fold(t.name) + " " + library::fold(nlib_flow::facts(t.id).publisher), words)) continue;
            if (t.id) at[t.id] = hits.size();
            hits.push_back({ { l, i }, { s.name } });
        }
    }
    // The console's games no server has.
    std::vector<insights::Played> own;
    if (const auto games = installed_flow::games())
        for (const installed_flow::Game& g : *games) {
            if (at.count(g.game.id)) continue;
            const std::string name = insights::name_of(g.game.id, g.name);
            if (has_words(library::fold(name), words)) own.push_back({ g, name, {} });
        }

    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::STRETCH);
    brls::View*              first = nullptr;
    std::vector<std::string> ids;
    auto header = [box](const std::string& title) {
        auto* h = new brls::Header();
        h->setTitle(title);
        h->setMarginTop(box->getChildren().empty() ? 0 : 24);
        h->setMarginBottom(8);
        box->addView(h);
    };
    if (hits.empty() && own.empty()) {
        kit::Empty empty = kit::empty("search");
        empty.title->setText(brls::getStr("playshelf/home/search_none", this->query));
        empty.text->setText(library::load_servers().empty() ? "playshelf/home/search_no_server"_i18n
                                                            : "playshelf/home/search_none_text"_i18n);
        box->addView(empty.box);
    }
    if (!hits.empty()) {
        header(hits.size() >= MAX_RESULTS ? brls::getStr("playshelf/home/search_many", MAX_RESULTS)
                          : hits.size() == 1    ? "playshelf/home/search_count_one"_i18n
                                                : brls::getStr("playshelf/home/search_count", hits.size()));
        for (const Hit& h : hits) {
            const library::Title&     t = h.pick.title();
            const library::TitleState st = library::state_of(t, h.pick.data->fetch.catalog, *installed);
            auto*                     cell = new brls::DetailCell();
            cell->setText(t.name);
            std::string servers;
            for (const std::string& s : h.servers) servers += (servers.empty() ? "" : ", ") + s;
            std::string detail = st.base ? "playshelf/library/installed"_i18n : library_flow::size_text(t.size);
            cell->setDetailText(detail + "  ·  " + servers);
            cell->detail->setThemeColor("textColor", st.base ? "cadran/teal" : "cadran/text2");
            const auto   data = h.pick.data;
            const size_t index = h.pick.index;
            cell->registerClickAction([data, index](brls::View*) {
                game_page::open(data, index);
                return true;
            });
            ids.push_back("se_hit_" + std::to_string(ids.size()));
            cell->setId(ids.back());
            box->addView(cell);
            if (!first) first = cell;
        }
    }
    if (!own.empty()) {
        header("playshelf/home/search_console"_i18n);
        for (const insights::Played& p : own) {
            auto* cell = new brls::DetailCell();
            cell->setText(p.name);
            cell->setDetailText("playshelf/library/installed"_i18n);
            cell->detail->setThemeColor("textColor", "cadran/teal");
            cell->registerClickAction([](brls::View*) { return true; });
            ids.push_back("se_own_" + std::to_string(ids.size()));
            cell->setId(ids.back());
            box->addView(cell);
            if (!first) first = cell;
        }
    }
    std::vector<std::pair<const char*, const char*>> icons;
    for (const std::string& id : ids) icons.push_back({ id.c_str(), "gamepad" });
    cadran::List(box, box).apply(icons);
    root->addView(box);
    brls::Application::giveFocus(first ? first : (brls::View*)this->getContentView());
    if (this->content) root->removeView(this->content);
    this->content = box;
}
