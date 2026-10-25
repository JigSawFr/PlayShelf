// install_flow — Help & diagnostics › developer tools › Install from a URL:
// the address of a package on an HTTPS server (an NSP, NSZ, XCI or XCZ,
// served with ranges) asked with the keyboard, then the package installed
// to the SD card (install/installer.hpp), a dialog showing what is written
// and Cancel. A content not signed by Nintendo stops it; the user may then
// install it anyway. Refused in read-only mode. The desktop build installs
// to its simulated console (sim/sim_install.cpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace install_flow
{

void from_url();

}   // namespace install_flow
