// library/titles.cpp: a catalogue's items gathered by game (a base game, its
// updates newest first, its add-ons; an item without a title id on its
// own), each game's name, icon and date; the search (case, accents, a title
// id), a section, the three orders.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "check.h"
#include "library/titles.hpp"

using namespace library;

namespace
{
Item item(const std::string& name, uint64_t id, uint32_t version, uint64_t size, const std::string& section = "s")
{
    Item it;
    it.name = name;
    it.url = "https://h/" + name;
    it.title_id = id;
    it.version = version;
    it.kind = id ? kind_of(id) : Kind::Unknown;
    it.size = size;
    it.section = section;
    return it;
}

const Title* find(const std::vector<Title>& titles, uint64_t id)
{
    for (const Title& t : titles)
        if (t.id == id) return &t;
    return nullptr;
}

void test_group()
{
    Catalog c;
    c.items.push_back(item("Zelda Update [0100000000010800][v65536].nsp", 0x0100000000010800, 65536, 10, "updates"));
    c.items.push_back(item("Zelda [0100000000010000][v0].nsp", 0x0100000000010000, 0, 1000));
    c.items.push_back(item("Zelda Update 2 [0100000000010800][v131072].nsp", 0x0100000000010800, 131072, 20, "updates"));
    c.items.push_back(item("Zelda DLC B", 0x0100000000011002, 0, 5));
    c.items.push_back(item("Zelda DLC A", 0x0100000000011001, 0, 5));
    c.items.push_back(item("homebrew.nsp", 0, 0, 7));
    c.items.push_back(item("Mario Update [0100000000020800][v65536].nsz", 0x0100000000020800, 65536, 3));
    c.items[1].icon_url = "https://h/zelda.jpg";
    c.items[1].release_date = 20170303;
    c.titles[0x0100000000020000] = {"Super Mario", 65536};

    const std::vector<Title> titles = group_titles(c);
    CHECK(titles.size() == 3);
    const Title* zelda = find(titles, 0x0100000000010000);
    if (!CHECK(zelda)) return;
    CHECK(zelda->name == "Zelda");
    CHECK(zelda->base == 1);
    CHECK(zelda->updates == std::vector<size_t>({2, 0}));
    CHECK(latest_update(*zelda) == 2);
    CHECK(zelda->dlcs == std::vector<size_t>({4, 3}));
    CHECK(zelda->size == 1040);
    CHECK(zelda->icon_url == "https://h/zelda.jpg");
    CHECK(zelda->release_date == 20170303);
    CHECK(zelda->sections == std::vector<std::string>({"updates", "s"}));

    const Title* mario = find(titles, 0x0100000000020000);
    if (!CHECK(mario)) return;
    CHECK(mario->name == "Super Mario");   // the titledb's
    CHECK(mario->base == -1);
    CHECK(latest_update(*mario) == 6);

    const Title* hb = find(titles, 0);
    if (!CHECK(hb)) return;
    CHECK(hb->name == "homebrew");
    CHECK(hb->base == 5);
    CHECK(latest_update(*hb) == -1);
}

void test_two_bases()
{
    Catalog c;
    c.items.push_back(item("Game v1", 0x0100000000030000, 65536, 1));
    c.items.push_back(item("Game v0", 0x0100000000030000, 0, 1));
    const std::vector<Title> titles = group_titles(c);
    if (!CHECK(titles.size() == 1)) return;
    CHECK(titles[0].base == 0);
    CHECK(titles[0].dlcs == std::vector<size_t>({1}));
}

void test_names()
{
    CHECK(fold("Pokémon ÉCARLATE Œuvre") == "pokemon ecarlate oeuvre");
    CHECK(fold("ゼルダ") == "ゼルダ");
    CHECK(fold("Straße") == "strasse");
    CHECK(clean_name("Game [0100ABCD00010000][v0].nsp") == "Game");
    CHECK(clean_name("Game (EU) [v0].XCI") == "Game (EU)");
    CHECK(clean_name("archive.tar") == "archive.tar");
    CHECK(clean_name("[only tags]") == "[only tags]");
}

void test_select()
{
    std::vector<Title> t(4);
    t[0].name = "Bravo";
    t[0].size = 10;
    t[0].release_date = 20200101;
    t[0].sections = {"a"};
    t[1].name = "alpha";
    t[1].size = 30;
    t[1].sections = {"b"};
    t[2].name = "Écho";
    t[2].size = 20;
    t[2].release_date = 20220101;
    t[2].sections = {"a", "b"};
    t[3].name = "Delta";
    t[3].id = 0x01006F8002326000;
    t[3].size = 20;
    t[3].sections = {"b"};

    CHECK(select_titles(t, "", Sort::Name) == std::vector<size_t>({1, 0, 3, 2}));
    CHECK(select_titles(t, "", Sort::Largest) == std::vector<size_t>({1, 3, 2, 0}));
    CHECK(select_titles(t, "", Sort::Newest) == std::vector<size_t>({2, 0, 1, 3}));
    CHECK(select_titles(t, "ECHO", Sort::Name) == std::vector<size_t>({2}));
    CHECK(select_titles(t, "a", Sort::Name, "a") == std::vector<size_t>({0}));
    CHECK(select_titles(t, "", Sort::Name, "b") == std::vector<size_t>({1, 3, 2}));
    CHECK(select_titles(t, "6f80023", Sort::Name).empty());   // too short for an id
    CHECK(select_titles(t, "01006F8002326000", Sort::Name) == std::vector<size_t>({3}));
    CHECK(select_titles(t, "zzz", Sort::Name).empty());
}

// By what is known of them: their genre, publisher, players, language,
// demo, and what the console has; sorted by publisher, the date known.
void test_filter()
{
    std::vector<Title> t(4);
    const char* const NAMES[] = { "Alpha", "Bravo", "Charlie", "Delta" };
    for (size_t i = 0; i < t.size(); i++) {
        t[i].name = NAMES[i];
        t[i].id = 0x0100000000010000 + 0x2000 * i;
    }
    std::map<uint64_t, Facts> known;
    known[t[0].id].publisher = "Zeta Games";
    known[t[0].id].categories = { "Puzzle" };
    known[t[0].id].players = 1;
    known[t[0].id].languages = { "en", "fr" };
    known[t[0].id].release_date = 20240101;
    known[t[1].id].publisher = "Acme";
    known[t[1].id].categories = { "Racing", "Action" };
    known[t[1].id].players = 4;
    known[t[1].id].languages = { "en" };
    known[t[2].id].publisher = "Acme";
    known[t[2].id].categories = { "Action" };
    known[t[2].id].demo = true;
    known[t[2].id].players = 2;
    Selection sel;
    sel.facts = [&](uint64_t id) {
        const auto it = known.find(id);
        return it == known.end() ? Facts() : it->second;
    };
    auto pick = [&](Sort sort = Sort::Name) { return select_titles(t, "", sort, "", sel); };

    CHECK(pick() == std::vector<size_t>({ 0, 1, 2, 3 }) && sel.filter.active() == 0);
    // Delta's facts are not known: a filter on them leaves it out.
    sel.filter.genre = "Action";
    CHECK(pick() == std::vector<size_t>({ 1, 2 }) && sel.filter.active() == 1);
    sel.filter.hide_demos = true;
    CHECK(pick() == std::vector<size_t>({ 1 }) && sel.filter.active() == 2);
    sel.filter = Filter();
    sel.filter.publisher = "Acme";
    CHECK(pick() == std::vector<size_t>({ 1, 2 }));
    sel.filter = Filter();
    sel.filter.players = 1;
    CHECK(pick() == std::vector<size_t>({ 0 }));
    sel.filter.players = 2;
    CHECK(pick() == std::vector<size_t>({ 1, 2 }));
    sel.filter = Filter();
    sel.filter.language = "fr";
    CHECK(pick() == std::vector<size_t>({ 0 }));

    // By publisher, those with none last; by date, the facts' when the
    // catalogue has none.
    sel.filter = Filter();
    CHECK(pick(Sort::Publisher) == std::vector<size_t>({ 1, 2, 0, 3 }));
    t[1].release_date = 20250101;
    CHECK(pick(Sort::Newest) == std::vector<size_t>({ 1, 0, 2, 3 }));

    // What the console has: applied when the list says.
    sel.filter.state = Filter::State::Installed;
    CHECK(pick().size() == 4);   // no state known: not applied
    sel.state = [](const Title& x) {
        TitleState st;
        st.base = x.name == "Bravo" || x.name == "Delta";
        st.update_available = x.name == "Delta";
        st.dlcs_missing = x.name == "Bravo" ? 2 : 0;
        return st;
    };
    CHECK(pick() == std::vector<size_t>({ 1, 3 }));
    sel.filter.state = Filter::State::Missing;
    CHECK(pick() == std::vector<size_t>({ 0, 2 }));
    sel.filter.state = Filter::State::ToUpdate;
    CHECK(pick() == std::vector<size_t>({ 3 }));
    sel.filter.state = Filter::State::DlcsMissing;
    CHECK(pick() == std::vector<size_t>({ 1 }));
}
}   // namespace

void test_state()
{
    Catalog c;
    c.items.push_back(item("Zelda", 0x0100000000010000, 0, 1000));
    c.items.push_back(item("Zelda Update", 0x0100000000010800, 131072, 10));
    c.items.push_back(item("Zelda DLC A", 0x0100000000011001, 0, 5));
    c.items.push_back(item("Zelda DLC B", 0x0100000000011002, 0, 5));
    const std::vector<Title> titles = group_titles(c);
    if (!CHECK(titles.size() == 1)) return;
    const Title& t = titles[0];

    std::map<uint64_t, uint32_t> installed;
    TitleState                   st = state_of(t, c, installed);
    CHECK(!st.base && !st.update_available && st.dlcs_missing == 0);

    installed[0x0100000000010000] = 0;
    st = state_of(t, c, installed);
    CHECK(st.base && st.version == 0 && st.update_available && st.dlcs_missing == 2);

    installed[0x0100000000010800] = 65536;
    installed[0x0100000000011002] = 0;
    st = state_of(t, c, installed);
    CHECK(st.version == 65536 && st.update_available && st.dlcs_missing == 1);
    CHECK(!is_installed(c.items[1], installed));
    CHECK(is_installed(c.items[3], installed));

    installed[0x0100000000010800] = 196608;   // newer than the server's
    st = state_of(t, c, installed);
    CHECK(st.version == 196608 && !st.update_available);
    CHECK(is_installed(c.items[1], installed));

    Title unknown;   // no title id: nothing known of it
    CHECK(!state_of(unknown, c, installed).base);
}

int main()
{
    test_group();
    test_two_bases();
    test_names();
    test_select();
    test_filter();
    test_state();
    return CHECK_DONE("library titles: items by game, names, search, sections, orders and what is installed passed");
}
