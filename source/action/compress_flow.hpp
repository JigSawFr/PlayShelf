// compress_flow — the developer tool that compresses a game installed on the
// console with the firmware's own compression layer (install/compress_title):
// pick a game, see what it would save, confirm, wait, then keep the result or
// go back to the original. Hidden behind developer mode (Help & tools), with
// English texts: a tool to try the engine on a console, not the screen of the
// feature (which will replace it).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

namespace compress_flow
{

// The developer button: picks a game, then goes through the steps above.
void open();

// At start-up: a compression a restart interrupted. Its journal says what to do:
// an unfinished one is undone, one that waits for the user's decision (the new
// copy is in place, the old files kept) asks again to keep it or go back.
void recover_at_start();

}   // namespace compress_flow
