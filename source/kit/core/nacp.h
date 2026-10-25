// nacp — a game's control data (its control.nacp and icon), shared by the
// play data layer (playstats.c) and the installed games check (gamecheck.c)
// so both name a game the same way. nacp.c never talks to a service (its
// name from the title entries, compressed ones of 21.0.0+ included, and the
// UTF-8 helpers): the host tests run it against a fake <switch.h>.
// nacp_read.c reads the control data and the console language (ns, set).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once
#include "nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// `s` is valid UTF-8 for its first `len` bytes (no overlong form, surrogate
// or code point above U+10FFFF).
bool nacp_utf8_valid(const char *s, size_t len);
// Copies at most n-1 bytes of a UTF-8 string without cutting a character.
void nacp_copy_utf8(char *dst, size_t n, const char *src);

// Byte 0x3215 of a control.nacp ([21.0.0+] TitlesDataFormat): 1 when the
// title block (0x3000 bytes at 0) is {u16 size; raw DEFLATE} that inflates to
// up to 32 language entries of 0x300 bytes.
#define NACP_SIZE          0x4000
#define NACP_TITLES_FORMAT 0x3215
// RatingAge: 0x20 signed bytes from 0x3040, the age each rating organisation
// (nn::ns::RatingOrganization, pctl_rating_org_name's order) rates the game
// for, -1 when it does not.
#define NACP_RATING_AGE    0x3040
#define NACP_RATING_COUNT  0x20

// The game's name from its control.nacp (`size` bytes, at least NACP_SIZE):
// the entry of language `lang` (an index into the title entries,
// nacp_console_language), else the first one with a name or a publisher, as
// nacpGetLanguageEntry() picks it. False, `out` empty, when there is none or
// it is not valid UTF-8 (the UI then names the game by its title ID).
bool nacp_name(const void *nacp, size_t size, u32 lang, char *out, size_t out_size);

// The game's ages from its control.nacp (`size` bytes, at least NACP_SIZE)
// into `out` (NACP_RATING_COUNT of them); false, every one -1, when too short.
bool nacp_rating_ages(const void *nacp, size_t size, int8_t *out);

// nacp_read.c, on the console (ns initialized by the caller): the control
// data of `app_id` into `control`, an NsApplicationControlData, ns' cache
// first (quick, what the HOME menu itself draws, and it still has a game
// card not inserted or an archived game), then the storage. With `icon`,
// only data that holds one. *size: the bytes read.
bool nacp_read_control(u64 app_id, void *control, bool icon, u64 *size);

// The console language, 0 when it cannot be read, else its SetLanguage + 1
// (what PlayStats.names_lang keeps), and its title entry in a control.nacp
// for nacp_name (American English when unknown), as libnx's
// nacpGetLanguageEntry() maps them.
u8 nacp_console_language(u32 *title_index);

#ifdef __cplusplus
}
#endif
