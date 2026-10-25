// cadran::List — the Cadran look for the list screens (the tabs' lines under
// their tops, every other screen) without rewriting them: the original
// tab is built as usual (its XML, its i18n strings, its actions), then its
// lines are restyled after the boards' settings lists. A section header
// becomes a kicker (kit::TEXT_KICKER, capitals, text2); each run of cells a
// card (surf, 1 px border, kit::RADIUS_CARD, a line between rows); a cell
// gets an icon in a soft square before its title, a chevron when it opens
// something, a switch for a BooleanCell; the notes go to kit::TEXT_NOTE. tiles() turns a few cells into
// the boards' tiles instead (a row of cards; the icon on top, or before an action's title
// over two soft lights in its own ink).
//
// The tab keeps every view it binds (they move, they are not rebuilt), and
// its own show / hide logic: a card or a tile row with all its lines hidden
// draws nothing and takes no room. The restyle adds no words of its own.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <string>
#include <utility>
#include <vector>

namespace cadran
{
// "Sécurité" -> "SÉCURITÉ": ASCII, Latin-1, œ and Cyrillic (the catalogs' scripts
// with a case); anything else is kept.
std::string upper(const std::string& text);

// A card of the lists: a rounded surface with a 1 px border (`accent`, the
// line colour unless set) drawn behind its rows, the rows' separators kept
// on all but the last visible one. Nothing is drawn when every row is hidden.
class Card : public brls::Box
{
  public:
    Card();
    void setAccent(NVGcolor color);
    void setFill(NVGcolor color);
    // Gone with its last row (and back with its first), so that its top
    // margin leaves no gap between the cards around it.
    void follow_rows();
    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

  private:
    NVGcolor fill, border;
};

// The boards' switch, drawn from a BooleanCell's state each frame (so it
// follows the cell whoever changes it).
class Switch : public brls::View
{
  public:
    explicit Switch(brls::BooleanCell* cell);
    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;

  private:
    brls::BooleanCell* cell;
    NVGcolor on, off, knob_on, knob_off;
};

class List
{
  public:
    // A screen inflated from one of the list XMLs (a ScrollView and its
    // column): a tab (`sub_screen` false: the rail at its left) or a screen
    // of its own (wider margins).
    explicit List(brls::View* tab, bool sub_screen = false);
    // A column built by its screen's own code (`column`, under `screen`, whose
    // ids it looks up), its padding left as it is: a list rebuilt as its
    // entries change dresses each new column before showing it.
    List(brls::View* screen, brls::Box* column);

    // The column of lines (nullptr when the tab has none).
    brls::Box* content() const { return this->box; }

    // Turns these cells (direct lines of the column) into tiles on one row,
    // where the first one was: `status` tiles show a reading (title small,
    // value large, no chevron), the others an action (icon then title on one
    // line, its words on the next, each smaller rather than on two). Before
    // apply(). Returns the row.
    brls::Box* tiles(const std::vector<std::pair<const char*, const char*>>& cells, bool status);

    // A status tile's state beside its value, by its shape and colour as
    // Today's pills: "cadran/teal_fill" a check (as it should be),
    // "cadran/amber_fill" a warning (needs the parent); nullptr: none.
    void state(const char* id, const char* color_key);

    // A status tile's pill under its value ("23.0.1 available"; empty: none):
    // in amber on its amber ground, the tile outlined in amber, when `warn`
    // (something to do), else in the calm pill colours ("Up to date"); and
    // `note` under it, smaller ("2 versions behind"; empty: none).
    void tag(const char* id, const std::string& text, bool warn, const std::string& note = "");

    // A tile's head (its icon's row, an action tile's title in it too), for a
    // sign at its far end (RowIcon, TabBadge): the tile's corner, not under
    // its value.
    brls::Box* head(const char* id) const;

    // Puts this line (not a cell: a box of text) in a card of its own.
    Card* wrap(const char* id);

    // Restyles every other line; `icons`: cell id -> icon name. A line that
    // opens a screen keeps the app's own "›" at the end of its value.
    void apply(const std::vector<std::pair<const char*, const char*>>& icons);

    // The card holding this cell (after apply()).
    Card* card_of(const char* id) const;

    // After the tab's refresh(): the headers' titles in capitals again (a tab
    // may set them, e.g. About's "What's new in …"), and a status tile's
    // plain value in the text colour (its warnings keep theirs).
    void sync();

    // For the lists a screen fills itself (its own code finds its lines where
    // it put them, so nothing is moved): a section title, and a cell as a row
    // of a card (apply()'s own), or as a card of its own (`alone`).
    static void section(brls::Header* header);
    static void row(brls::DetailCell* cell, const char* icon = nullptr, bool alone = false);
    // The same with a picture in its icon's place (a resource, "img/…png":
    // a language's flag), drawn as it is, without the square.
    static void row_image(brls::DetailCell* cell, const std::string& image);
    // A row's icon (row()'s, at its start) changed: another `icon` (nullptr:
    // the same), in `color_key`, on `square_key` (nullptr: the usual square).
    static void recolor_icon(brls::DetailCell* cell, const char* icon, const char* color_key,
                         const char* square_key = nullptr);
    // The box a screen's rows go in: a card in `list` (made the first time).
    // The screen clears and fills that one.
    static brls::Box* rows_in(brls::Box* list);

  private:
    brls::View* tab;
    brls::Box*  box = nullptr;
    std::vector<brls::Header*> headers;
    std::vector<brls::DetailCell*> readings;   // the status tiles
    std::vector<std::pair<brls::DetailCell*, brls::Box*>>  heads;   // every tile's head (head())
    std::vector<std::pair<brls::DetailCell*, brls::View*>> marks;   // each one's state (a kit::Mark)
    std::vector<std::pair<brls::DetailCell*, brls::Box*>>  tags;    // each one's pill (tag())
};
}   // namespace cadran
