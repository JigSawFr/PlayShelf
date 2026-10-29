// MainActivity — root screen: a TabFrame (sidebar of tabs) described in
// resources/xml/activity/main.xml. The tabs live in source/tab/.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>

class MainActivity : public aurore::Activity
{
  public:
    // activity/main.xml.
    aurore::View* createContentView() override;

    void onContentAvailable() override;
    void onResume() override;

    ~MainActivity() override;

    // "PlayShelf", and the header's pill again (read-only says so on every
    // tab).
    void update_title();

    // Shows tab `position` (tabs.hpp) and focuses its content: the view
    // with id `row` when given and shown, else its first one.
    void open_tab(int position, const std::string& row = "");

  private:
    // The notification bell: Y opens its list, the footer says how many
    // entries are to do.
    int  notice_listener = 0;
    void update_notice_hint();
    // Home's badge and the bell's entries, from action/insights.
    int  insights_listener = 0;
    void update_insights();
};
