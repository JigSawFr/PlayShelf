// install_flow — Help & diagnostics › developer tools › Install from a URL:
// the address of a package on an HTTPS server (an NSP, NSZ, XCI or XCZ,
// served with ranges) asked with the keyboard, then the package installed
// to the SD card (install/installer.hpp), a dialog showing what is written
// and Cancel. A content not signed by Nintendo stops it; the user may then
// install it anyway. Refused in read-only mode. The desktop build installs
// to its simulated console (sim/sim_install.cpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace install_flow
{

void from_url();

// A package to install: its name as shown, its address, the headers sent
// with each request (a library server's Authorization) and the pin of its
// server's key (a LAN server's own certificate; "" for the CA store).
struct Package
{
    std::string              name, url;
    std::vector<std::string> headers;
    std::string              pin;
};

// These one after the other in the same dialog ("2 of 3"), as from_url()
// does one: a failure stops there (the packages installed before it stay);
// a content not signed by Nintendo asks, and installed anyway, the ones
// after it are too. `after`, once the dialog has closed (whatever came
// of it): what was installed changed.
void install(const std::vector<Package>& packages, std::function<void()> after = nullptr);

}   // namespace install_flow
