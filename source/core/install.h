// core/install — the console services an install goes through, in C: ncm's
// content storage (placeholders, contents) and content meta database, ns's
// application records, es's tickets, and spl's keys for NCA headers and key
// areas. install/console_core.cpp puts them behind install::Console.
//
// On the Switch this is libnx (install.c), es's ImportTicket and ns's
// PushApplicationRecord over IPC as the installers it comes from call them;
// the desktop build swaps in a console in a folder (sim/sim_install.cpp)
// whose keys are the host tests' (tests/install/builders.hpp).
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include "kit/core/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CoreStorage_Nand = 4,   // NcmStorageId_BuiltInUser
    CoreStorage_Sd = 5,     // NcmStorageId_SdCard
} CoreStorage;

// NcmContentMetaKey's layout.
typedef struct {
    u64 id;
    u32 version;
    u8  type;
    u8  install_type;
    u8  padding[2];
} CoreMetaKey;

// Opens the services and the storage installed to; every call below needs
// it. Opening again with another storage closes the first.
Result core_install_open(CoreStorage storage);
void   core_install_close(void);

// A content and a placeholder are named by the same 16-byte id.
Result core_content_has(const u8 id[16], bool *out);
Result core_placeholder_create(const u8 id[16], u64 size);
Result core_placeholder_write(const u8 id[16], u64 offset, const void *data, size_t size);
Result core_placeholder_register(const u8 id[16]);
void   core_placeholder_delete(const u8 id[16]);
void   core_content_delete(const u8 id[16]);
Result core_free_space(u64 *out);

// The content meta database. core_meta_get: the record's size in *size
// (buf may be NULL), then up to buf_size bytes of it into buf; *found
// false when there is none.
Result core_meta_get(const CoreMetaKey *key, void *buf, u64 buf_size, u64 *size, bool *found);
Result core_meta_set(const CoreMetaKey *key, const void *record, u64 size);
Result core_meta_remove(const CoreMetaKey *key);
Result core_meta_commit(void);

Result core_push_application_record(u64 application_id, const CoreMetaKey *key);
Result core_import_ticket(const void *ticket, size_t ticket_size, const void *cert, size_t cert_size);

// The NCA header key, and one key of a header's key area decrypted with
// the key area key of `generation` (0-based) and `index` (0 application,
// 1 ocean, 2 system).
Result core_header_key(u8 out[32]);
Result core_unwrap_key(u8 generation, u8 index, const u8 in[16], u8 out[16]);

// Every content meta installed, on the SD card then in the console's
// memory (a storage that cannot be opened, such as a missing SD card, is
// skipped): up to `max` keys into `out`, how many in *count. Needs no
// core_install_open().
Result core_installed_list(CoreMetaKey *out, int max, int *count);

// hosversionGet().
u32 core_system_version(void);

#ifdef __cplusplus
}
#endif
