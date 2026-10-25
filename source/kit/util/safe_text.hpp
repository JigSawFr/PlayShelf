// safe_text — text from outside (GitHub: a release's or a pull request's
// title, notes, branch) made safe to show: only valid UTF-8 is kept, and
// none of the characters that could make it lie about itself or upset the
// layout. Plain C++ for the host tests (tests/markdown).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace safe_text
{

// `text` with:
// - each byte that is not part of a valid UTF-8 sequence (a stray one, an
//   overlong form, a surrogate, past U+10FFFF) left out;
// - the control characters (C0 but the line feed, DEL, C1) left out, a tab
//   and the line and paragraph separators (U+2028, U+2029) turned into a
//   space and a line feed, a carriage return dropped (CRLF becomes LF);
// - the invisible characters that reorder or hide text left out: the bidi
//   embeddings, overrides and isolates (U+202A–U+202E, U+2066–U+2069), the
//   marks (U+200E, U+200F, U+061C), the zero-width space, non-joiner and
//   word joiner (U+200B, U+200C, U+2060) and the byte-order mark (U+FEFF);
// - line feeds turned into spaces unless `multiline`;
// - at most `max_bytes` bytes, cut between two characters.
std::string clean(const std::string& text, size_t max_bytes, bool multiline);

// Release notes, a description: clean() on several lines, then at most
// `max_bytes` bytes, cut at the end of a whole line ("" when even the first
// one is longer).
std::string clean_notes(const std::string& text, size_t max_bytes);

// A version as releases name it ("1.2.0", "1.3.0-rc.1", "2.0.0+build.5"):
// 1 to 40 of [0-9A-Za-z.+-_], starting with a digit or a letter.
bool is_version(const std::string& text);

}   // namespace safe_text
