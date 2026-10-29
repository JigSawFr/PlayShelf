// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/error_hint_logic.hpp"

#include "core/nx_types.h"

#include <iterator>

namespace error_hint_logic
{

const char* hint_key(uint32_t rc)
{
    if (NXM_IS_APP_RESULT(rc)) {
        // By description: NXM_RC_READ_ONLY is 1.
        static const char* const OWN[] = {
            "",                   // 0: not used
            "read_only",          // NXM_RC_READ_ONLY
            "not_confirmed",      // NXM_RC_NOT_CONFIRMED
            "invalid_argument",   // NXM_RC_INVALID_ARGUMENT
            "fw_unsupported",     // NXM_RC_FW_UNSUPPORTED
            "not_saved",          // NXM_RC_NOT_SAVED
            "cancelled",          // NXM_RC_CANCELLED
        };
        const uint32_t desc = NXM_RESULT_DESC(rc);
        return desc < std::size(OWN) ? OWN[desc] : "";
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
