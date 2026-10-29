// SearchActivity — Home's search: the games whose name (or publisher) has
// the words asked, every server read in this run together, then those of
// the console no server has. Each game once, with the servers that have it
// and what the console has of it; A opens its page. Y asks other words.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <string>

class SearchActivity : public aurore::Activity
{
  public:
    explicit SearchActivity(std::string query);
    CONTENT_FROM_XML_RES("activity/search.xml");

    void onContentAvailable() override;

    // The keyboard, then the results (nothing when no words are given).
    static void ask(const std::string& initial = "");

    static constexpr size_t MAX_RESULTS = 120;

  private:
    std::string query;
    aurore::Box*  content = nullptr;

    void rebuild();

    AURORE_BIND(aurore::Box, root, "se_root");
};
