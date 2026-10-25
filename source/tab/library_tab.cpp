// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "tab/library_tab.hpp"

#include "ui/cadran_frame.hpp"
#include "ui/cadran_kit.hpp"

using namespace brls::literals;

LibraryTab::LibraryTab()
    : TabBase("xml/tab/library.xml", false)
{
    cadran::kit::Empty empty = cadran::kit::empty("shelf");
    empty.title->setText("playshelf/library/empty_title"_i18n);
    empty.text->setText("playshelf/library/empty_text"_i18n);
    empty.box->setMarginTop(40);
    content->addView(empty.box);
}

void LibraryTab::refresh()
{
    cadran::set_pill("");
}

brls::View* LibraryTab::create()
{
    return new LibraryTab();
}
