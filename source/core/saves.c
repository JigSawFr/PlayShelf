// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "core/saves.h"

#include <stdio.h>
#include <string.h>
#include <switch.h>

#define MOUNT_NAME "pssave"

int core_save_users(CoreSaveUser *out, int max)
{
    if (R_FAILED(accountInitialize(AccountServiceType_Application))) return -1;
    AccountUid uids[ACC_USER_LIST_SIZE];
    s32        total = 0;
    int        n = -1;
    if (R_SUCCEEDED(accountListAllUsers(uids, ACC_USER_LIST_SIZE, &total))) {
        n = 0;
        for (s32 i = 0; i < total && n < max; i++) {
            if (!accountUidIsValid(&uids[i])) continue;
            CoreSaveUser *u = &out[n++];
            memset(u, 0, sizeof(*u));
            u->uid[0] = uids[i].uid[0];
            u->uid[1] = uids[i].uid[1];
            AccountProfile     profile;
            AccountProfileBase base;
            if (R_SUCCEEDED(accountGetProfile(&profile, uids[i]))) {
                memset(&base, 0, sizeof(base));
                if (R_SUCCEEDED(accountProfileGet(&profile, NULL, &base))) {
                    size_t len = strnlen(base.nickname, sizeof(base.nickname));
                    if (len >= CORE_SAVE_NAME_MAX) len = CORE_SAVE_NAME_MAX - 1;
                    memcpy(u->name, base.nickname, len);
                    u->name[len] = 0;
                }
                accountProfileClose(&profile);
            }
        }
    }
    accountExit();
    return n;
}

int core_save_users_all(CoreSaveUser *out, int max)
{
    if (max < 3) return core_save_users(out, max);
    const int n = core_save_users(out, max - 2);
    if (n < 0) return n;
    memset(&out[n], 0, 2 * sizeof(out[0]));
    out[n].uid[1] = CORE_SAVE_DEVICE;
    out[n + 1].uid[1] = CORE_SAVE_BCAT;
    return n + 2;
}

int core_save_titles(const CoreSaveUser *user, u64 *out, int max)
{
    const int           shared = core_save_shared_kind(user);
    const FsSaveDataType want = shared == CORE_SAVE_DEVICE ? FsSaveDataType_Device
                              : shared == CORE_SAVE_BCAT   ? FsSaveDataType_Bcat
                                                           : FsSaveDataType_Account;
    FsSaveDataInfoReader reader;
    if (R_FAILED(fsOpenSaveDataInfoReader(&reader, FsSaveDataSpaceId_User))) return -1;
    int  n = 0;
    bool failed = false;
    for (;;) {
        FsSaveDataInfo info[32];
        s64            got = 0;
        if (R_FAILED(fsSaveDataInfoReaderRead(&reader, info, 32, &got))) {
            failed = true;
            break;
        }
        if (got <= 0) break;
        for (s64 i = 0; i < got && n < max; i++) {
            if (info[i].save_data_type != want || !info[i].application_id) continue;
            if (!shared && (info[i].uid.uid[0] != user->uid[0] || info[i].uid.uid[1] != user->uid[1])) continue;
            bool seen = false;
            for (int k = 0; k < n && !seen; k++) seen = out[k] == info[i].application_id;
            if (!seen) out[n++] = info[i].application_id;
        }
    }
    fsSaveDataInfoReaderClose(&reader);
    return failed && n == 0 ? -1 : n;
}

// A save with no account behind it (device, BCAT) opened by its attributes and mounted as a device.
static Result mount_shared(int shared, u64 app_id, bool writable, char *root, size_t root_size)
{
    // The system manages the data it downloaded for a game: it is read, never written.
    if (shared == CORE_SAVE_BCAT && writable) return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    FsSaveDataAttribute attr;
    memset(&attr, 0, sizeof(attr));
    attr.application_id = app_id;
    attr.save_data_type = shared == CORE_SAVE_BCAT ? FsSaveDataType_Bcat : FsSaveDataType_Device;
    FsFileSystem fs;
    Result       rc = writable ? fsOpenSaveDataFileSystem(&fs, FsSaveDataSpaceId_User, &attr)
                               : fsOpenReadOnlySaveDataFileSystem(&fs, FsSaveDataSpaceId_User, &attr);
    if (R_FAILED(rc)) return rc;
    if (fsdevMountDevice(MOUNT_NAME, fs) < 0) {
        fsFsClose(&fs);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    snprintf(root, root_size, "%s:/", MOUNT_NAME);
    return 0;
}

Result core_save_mount(const CoreSaveUser *user, u64 app_id, bool writable, char *root, size_t root_size)
{
    if (core_save_shared_kind(user)) return mount_shared(core_save_shared_kind(user), app_id, writable, root, root_size);
    AccountUid uid;
    uid.uid[0] = user->uid[0];
    uid.uid[1] = user->uid[1];
    const Result rc = writable ? fsdevMountSaveData(MOUNT_NAME, app_id, uid)
                               : fsdevMountSaveDataReadOnly(MOUNT_NAME, app_id, uid);
    if (R_SUCCEEDED(rc)) snprintf(root, root_size, "%s:/", MOUNT_NAME);
    return rc;
}

Result core_save_commit(void)
{
    return fsdevCommitDevice(MOUNT_NAME);
}

void core_save_unmount(void)
{
    fsdevUnmountDevice(MOUNT_NAME);
}
