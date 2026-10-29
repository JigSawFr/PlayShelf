// DiagnosticActivity — developer mode: the diagnostic report (firmware, clocks,
// every pctl query) read on the console itself; Y saves it like Help & diagnostics ›
// Export a diagnostic report, X sends it online like Send a report online.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <aurore.hpp>
#include <string>

class DiagnosticActivity : public aurore::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/diagnostic.xml");

    void onContentAvailable() override;

  private:
    std::string report;

    AURORE_BIND(aurore::Box, list, "dg_list");
};
