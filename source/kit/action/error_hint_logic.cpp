// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/error_hint_logic.hpp"

#include "core/nx_types.h"

namespace error_hint_logic
{

const char* hint_key(uint32_t rc)
{
    if (NXM_IS_APP_RESULT(rc)) {
        // By description, whatever the number: the kit's NXM_RC_* are shared with other apps.
        switch (NXM_RESULT_DESC(rc)) {
            case NXM_RESULT_DESC(NXM_RC_READ_ONLY):        return "read_only";
            case NXM_RESULT_DESC(NXM_RC_NOT_CONFIRMED):    return "not_confirmed";
            case NXM_RESULT_DESC(NXM_RC_INVALID_ARGUMENT): return "invalid_argument";
            case NXM_RESULT_DESC(NXM_RC_FW_UNSUPPORTED):   return "fw_unsupported";
            case NXM_RESULT_DESC(NXM_RC_NOT_SAVED):        return "not_saved";
            case NXM_RESULT_DESC(NXM_RC_CANCELLED):        return "cancelled";
            default: return "";
        }
    }
    switch (rc) {
        case 0xF601: return "session_closed";   // 2001-0123 SessionClosed
        // 2021-0008 (switchbrew: sm NotAllowed): no access to the service asked for.
        case 0x1015: return "service_denied";
        // 2016-0601 / 2016-0602 (Nintendo support): corrupt data when a game
        // starts; also what an integrity check answers when it finds it.
        case 0x4B210:
        case 0x4B410: return "corrupt_data";
        default: break;
    }
    return "";
}

}   // namespace error_hint_logic
