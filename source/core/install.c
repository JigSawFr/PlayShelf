// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
// ImportTicket (es, command 1) and PushApplicationRecord (ns's application
// manager, command 16), which libnx does not wrap, are called as Awoo and
// CyberFoil call them (Adubbz's Tinfoil, MIT).
#include "core/install.h"

#include <string.h>
#include <switch.h>

static bool               s_open;
static NcmContentStorage  s_storage;
static NcmContentMetaDatabase s_meta;
static NcmStorageId       s_storage_id;

// ns's NsApplicationRecordType (3: installed), which libnx does not name.
#define CORE_RECORD_INSTALLED 3
static Service            s_es;
static Service            s_ns_app;
static bool               s_ns_app_owned;

// The sources the keys are derived from (public, the same on every console).
static const u8 HEADER_KEK_SOURCE[16] = {0x1F, 0x12, 0x91, 0x3A, 0x4A, 0xCB, 0xF0, 0x0D,
                                         0x4C, 0xDE, 0x3A, 0xF6, 0xD5, 0x23, 0x88, 0x2A};
static const u8 HEADER_KEY_SOURCE[32] = {0x5A, 0x3E, 0xD8, 0x4F, 0xDE, 0xC0, 0xD8, 0x26, 0x31, 0xF7, 0xE2,
                                         0x5D, 0x19, 0x7B, 0xF5, 0xD0, 0x1C, 0x9B, 0x7B, 0xFA, 0xF6, 0x28,
                                         0x18, 0x3D, 0x71, 0xF6, 0x4D, 0x73, 0xF1, 0x50, 0xB9, 0xD2};
static const u8 KEY_AREA_KEY_SOURCE[3][16] = {
    {0x7F, 0x59, 0x97, 0x1E, 0x62, 0x9F, 0x36, 0xA1, 0x30, 0x98, 0x06, 0x6F, 0x21, 0x44, 0xC3, 0x0D},   // application
    {0x32, 0x7D, 0x36, 0x08, 0x5A, 0xD1, 0x75, 0x8D, 0xAB, 0x4E, 0x6F, 0xBA, 0xA5, 0x55, 0xD8, 0x82},   // ocean
    {0x87, 0x45, 0xF1, 0xBB, 0xA6, 0xBE, 0x79, 0x64, 0x7D, 0x04, 0x8B, 0xA6, 0x7B, 0x5F, 0xDA, 0x4A},   // system
};

static NcmContentId as_content(const u8 id[16])
{
    NcmContentId c;
    memcpy(c.c, id, 16);
    return c;
}

static NcmPlaceHolderId as_placeholder(const u8 id[16])
{
    NcmPlaceHolderId p;
    memcpy(p.uuid.uuid, id, 16);
    return p;
}

static NcmContentMetaKey as_key(const CoreMetaKey *k)
{
    NcmContentMetaKey m;
    memset(&m, 0, sizeof m);
    m.id = k->id;
    m.version = k->version;
    m.type = k->type;
    m.install_type = k->install_type;
    return m;
}

Result core_install_open(CoreStorage storage)
{
    if (s_open && s_storage_id == (NcmStorageId)storage) return 0;
    core_install_close();
    Result rc = ncmInitialize();
    if (R_FAILED(rc)) return rc;
    rc = ncmOpenContentStorage(&s_storage, (NcmStorageId)storage);
    if (R_SUCCEEDED(rc)) {
        rc = ncmOpenContentMetaDatabase(&s_meta, (NcmStorageId)storage);
        if (R_FAILED(rc)) ncmContentStorageClose(&s_storage);
    }
    if (R_FAILED(rc)) {
        ncmExit();
        return rc;
    }
    rc = smGetService(&s_es, "es");
    if (R_SUCCEEDED(rc)) rc = nsInitialize();
    if (R_SUCCEEDED(rc)) {
        if (hosversionBefore(3, 0, 0)) {
            s_ns_app = *nsGetServiceSession_ApplicationManagerInterface();
            s_ns_app_owned = false;
        } else {
            rc = nsGetApplicationManagerInterface(&s_ns_app);
            s_ns_app_owned = R_SUCCEEDED(rc);
        }
    }
    if (R_SUCCEEDED(rc)) rc = splCryptoInitialize();
    if (R_FAILED(rc)) {
        ncmContentMetaDatabaseClose(&s_meta);
        ncmContentStorageClose(&s_storage);
        ncmExit();
        if (serviceIsActive(&s_es)) serviceClose(&s_es);
        if (s_ns_app_owned) serviceClose(&s_ns_app);
        nsExit();
        return rc;
    }
    s_storage_id = (NcmStorageId)storage;
    s_open = true;
    return 0;
}

void core_install_close(void)
{
    if (!s_open) return;
    ncmContentMetaDatabaseClose(&s_meta);
    ncmContentStorageClose(&s_storage);
    ncmExit();
    serviceClose(&s_es);
    if (s_ns_app_owned) serviceClose(&s_ns_app);
    nsExit();
    splCryptoExit();
    s_open = false;
}

Result core_content_has(const u8 id[16], bool *out)
{
    const NcmContentId c = as_content(id);
    return ncmContentStorageHas(&s_storage, out, &c);
}

Result core_placeholder_create(const u8 id[16], u64 size)
{
    const NcmContentId     c = as_content(id);
    const NcmPlaceHolderId p = as_placeholder(id);
    return ncmContentStorageCreatePlaceHolder(&s_storage, &c, &p, (s64)size);
}

Result core_placeholder_write(const u8 id[16], u64 offset, const void *data, size_t size)
{
    const NcmPlaceHolderId p = as_placeholder(id);
    return ncmContentStorageWritePlaceHolder(&s_storage, &p, offset, data, size);
}

Result core_placeholder_register(const u8 id[16])
{
    const NcmContentId     c = as_content(id);
    const NcmPlaceHolderId p = as_placeholder(id);
    return ncmContentStorageRegister(&s_storage, &c, &p);
}

void core_placeholder_delete(const u8 id[16])
{
    const NcmPlaceHolderId p = as_placeholder(id);
    ncmContentStorageDeletePlaceHolder(&s_storage, &p);
}

void core_content_delete(const u8 id[16])
{
    const NcmContentId c = as_content(id);
    bool               has = false;
    if (R_SUCCEEDED(ncmContentStorageHas(&s_storage, &has, &c)) && has) ncmContentStorageDelete(&s_storage, &c);
}

Result core_free_space(u64 *out)
{
    s64          free_bytes = 0;
    const Result rc = ncmContentStorageGetFreeSpaceSize(&s_storage, &free_bytes);
    *out = R_SUCCEEDED(rc) && free_bytes > 0 ? (u64)free_bytes : 0;
    return rc;
}

Result core_meta_get(const CoreMetaKey *key, void *buf, u64 buf_size, u64 *size, bool *found)
{
    const NcmContentMetaKey k = as_key(key);
    bool                    has = false;
    Result                  rc = ncmContentMetaDatabaseHas(&s_meta, &has, &k);
    *found = false;
    *size = 0;
    if (R_FAILED(rc) || !has) return rc;
    rc = ncmContentMetaDatabaseGetSize(&s_meta, size, &k);
    if (R_FAILED(rc)) return rc;
    *found = true;
    if (!buf) return 0;
    u64 got = 0;
    return ncmContentMetaDatabaseGet(&s_meta, &k, &got, buf, buf_size < *size ? buf_size : *size);
}

Result core_meta_set(const CoreMetaKey *key, const void *record, u64 size)
{
    const NcmContentMetaKey k = as_key(key);
    return ncmContentMetaDatabaseSet(&s_meta, &k, record, size);
}

Result core_meta_remove(const CoreMetaKey *key)
{
    const NcmContentMetaKey k = as_key(key);
    return ncmContentMetaDatabaseRemove(&s_meta, &k);
}

Result core_meta_commit(void)
{
    return ncmContentMetaDatabaseCommit(&s_meta);
}

Result core_push_application_record(u64 application_id, const CoreMetaKey *key)
{
    // ContentStorageRecord: the meta key, then the storage id (u64).
    struct {
        NcmContentMetaKey key;
        u64               storage_id;
    } record;
    record.key = as_key(key);
    record.storage_id = s_storage_id;
    const struct {
        u8  last_modified_event;
        u64 application_id;
    } in = {CORE_RECORD_INSTALLED, application_id};
    return serviceDispatchIn(&s_ns_app, 16, in, .buffer_attrs = {SfBufferAttr_HipcMapAlias | SfBufferAttr_In},
                             .buffers = {{&record, sizeof record}});
}

Result core_import_ticket(const void *ticket, size_t ticket_size, const void *cert, size_t cert_size)
{
    return serviceDispatch(&s_es, 1,
                           .buffer_attrs = {SfBufferAttr_HipcMapAlias | SfBufferAttr_In, SfBufferAttr_HipcMapAlias | SfBufferAttr_In},
                           .buffers = {{ticket, ticket_size}, {cert, cert_size}});
}

Result core_header_key(u8 out[32])
{
    u8     kek[16];
    Result rc = splCryptoGenerateAesKek(HEADER_KEK_SOURCE, 0, 0, kek);
    if (R_SUCCEEDED(rc)) rc = splCryptoGenerateAesKey(kek, HEADER_KEY_SOURCE, out);
    if (R_SUCCEEDED(rc)) rc = splCryptoGenerateAesKey(kek, HEADER_KEY_SOURCE + 16, out + 16);
    return rc;
}

Result core_unwrap_key(u8 generation, u8 index, const u8 in[16], u8 out[16])
{
    if (index > 2) return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    u8     kek[16];
    Result rc = splCryptoGenerateAesKek(KEY_AREA_KEY_SOURCE[index], generation, 0, kek);
    if (R_SUCCEEDED(rc)) rc = splCryptoGenerateAesKey(kek, in, out);
    return rc;
}

_Static_assert(sizeof(CoreMetaKey) == sizeof(NcmContentMetaKey), "CoreMetaKey is NcmContentMetaKey's layout");

Result core_installed_list(CoreMetaKey *out, int max, int *count)
{
    *count = 0;
    const Result rc = ncmInitialize();
    if (R_FAILED(rc)) return rc;
    const NcmStorageId storages[2] = {NcmStorageId_SdCard, NcmStorageId_BuiltInUser};
    for (int i = 0; i < 2 && *count < max; i++) {
        NcmContentMetaDatabase db;
        if (R_FAILED(ncmOpenContentMetaDatabase(&db, storages[i]))) continue;
        s32 total = 0, written = 0;
        if (R_SUCCEEDED(ncmContentMetaDatabaseList(&db, &total, &written, (NcmContentMetaKey *)(out + *count), (s32)(max - *count),
                                                   NcmContentMetaType_Unknown, 0, 0, UINT64_MAX, NcmContentInstallType_Full)))
            *count += written;
        ncmContentMetaDatabaseClose(&db);
    }
    ncmExit();
    return 0;
}

u32 core_system_version(void)
{
    return hosversionGet();
}
