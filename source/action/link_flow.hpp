// link_flow — PlayShelf's side of the Boréale remote link: while the agent
// that holds the console's broker connection runs (the PlayGuard agent's
// `bl:link` service, boreale-link's bl_relay_nx), PlayShelf hands it its
// `state` document (link/link_state) to publish under
// boreale/<id>/playshelf/, and takes back the orders for its module (the
// pause button). Without an agent nothing is published: PlayShelf opens no
// connection of its own yet.
//
// Everything runs on the UI thread, one turn a second: the session is opened
// when the agent answers (looked for again every 15 s), the document rebuilt
// every 5 s and when an order was applied, and sent when it changed. A call
// that fails because the agent went away ends the session; PlayShelf looks
// for the agent again. A retained order is cleared on the broker once taken,
// so a reconnection does not replay it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace link_flow
{

// Starts the turns (once, when the main screen is up).
void start();

// While the agent is being installed, updated or stopped, PlayShelf leaves it
// alone: its session closes and none opens until `hold(false)`
// (boreale-kit's agent_manage_flow). The turns look for the agent again
// right away when it ends, whether it came back or not.
void hold(bool on);

}   // namespace link_flow
