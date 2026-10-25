// What every fake <switch.h> of the C suites starts with (tests/<suite>/
// switch.h, -DNX_HOST_TEST): libnx's integer types, Result with its tests
// and MAKERESULT, and the firmware version macros, as libnx defines them.
// Each suite's switch.h adds the services its code calls.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int64_t s64;
typedef u32 Result;

#define R_SUCCEEDED(rc) ((rc) == 0)
#define R_FAILED(rc) ((rc) != 0)
#define MAKERESULT(module, description) ((((module) & 0x1FF)) | ((description) & 0x1FFF) << 9)
#define MAKEHOSVERSION(_major, _minor, _micro) (((u32)(_major) << 16) | ((u32)(_minor) << 8) | (u32)(_micro))
#define HOSVER_MAJOR(_v) (((_v) >> 16) & 0xFF)
#define HOSVER_MINOR(_v) (((_v) >> 8) & 0xFF)
#define HOSVER_MICRO(_v) ((_v) & 0xFF)
