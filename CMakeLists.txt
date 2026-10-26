cmake_minimum_required(VERSION 3.10)

# Borealis lives in library/aurore as a git submodule: Aurore (JigSawFr/Aurore, main),
# xfangfang/borealis with our fixes and some from its other forks.
set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/library/aurore/library)

# On the Switch, draw with deko3d (devkitPro's own GPU API) rather than mesa's
# OpenGL: mesa (libEGL + libglapi) was ~5 MB of the 9.4 MB .nro and most of
# the start-up time. -DUSE_DEKO3D=OFF (make GL=1) builds the OpenGL variant.
# Declared before borealis' options so this default wins; ignored on desktop.
option(USE_DEKO3D "Switch: draw with deko3d instead of OpenGL (mesa)" ON)

# PLATFORM_SWITCH / PLATFORM_DESKTOP / USE_DEKO3D … options.
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

# Sets up the devkitPro cross-toolchain when PLATFORM_SWITCH=ON.
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

project(playshelf LANGUAGES CXX C)

# borealis' toolchain.cmake sets CMAKE_C_FLAGS / CMAKE_CXX_FLAGS as normal
# variables (include paths), which hides the cache values devkitPro's
# NintendoSwitch platform file initialises: the Cortex-A57 tuning, the crc and
# crypto extensions and the thread-local model libnx is built with. Put them
# back for every target (borealis, yoga, fmt, tinyxml2 and the app).
if (PLATFORM_SWITCH AND NX_ARCH_SETTINGS)
    string(PREPEND CMAKE_C_FLAGS "${NX_ARCH_SETTINGS} ")
    string(PREPEND CMAKE_CXX_FLAGS "${NX_ARCH_SETTINGS} ")
endif ()
# devkitPro's platform file defaults to Release (-O2 -g; elf2nro keeps no
# debug data). borealis prints its "Build Type:" line before project() picks
# that default, so say what is actually used.
# deko3d draws with precompiled shaders: borealis' nanovg loads them from
# romfs:/shaders/. uam (part of switch-dev) does not always give the same
# bytes for the same GLSL (one scheduling word of fill_vsh differs from run to
# run), which broke the reproducible zips, so the compiled shaders are kept in
# shaders/ and copied as they are. shaders/glsl.sha256 is the GLSL they were
# compiled from: after a borealis bump changes it, configure with
# -DPLAYSHELF_UAM=ON, copy build/dksh/*.dksh to shaders/ and update the checksums.
option(PLAYSHELF_UAM "Switch: compile the deko3d shaders with uam instead of using shaders/" OFF)
set(PLAYSHELF_DKSH_DIR ${CMAKE_BINARY_DIR}/dksh)
set(PLAYSHELF_SHADERS fill_vsh fill_fsh fill_aa_fsh)
if (PLATFORM_SWITCH AND USE_DEKO3D)
    if (PLAYSHELF_UAM)
        gen_dksh(${PLAYSHELF_DKSH_DIR})
        foreach (shader ${PLAYSHELF_SHADERS})
            if (NOT EXISTS ${PLAYSHELF_DKSH_DIR}/${shader}.dksh)
                message(FATAL_ERROR "uam did not produce ${shader}.dksh")
            endif ()
        endforeach ()
    else ()
        file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/glsl.sha256 PLAYSHELF_GLSL_SUMS)
        foreach (line ${PLAYSHELF_GLSL_SUMS})
            string(REGEX MATCH "^([0-9a-f]+)  (.+)$" _ "${line}")
            file(SHA256 ${BOREALIS_LIBRARY}/lib/extern/nanovg/deko3d/shaders/${CMAKE_MATCH_2} PLAYSHELF_GLSL_SUM)
            if (NOT PLAYSHELF_GLSL_SUM STREQUAL CMAKE_MATCH_1)
                message(FATAL_ERROR "borealis' ${CMAKE_MATCH_2} changed since shaders/ was compiled: "
                        "configure with -DPLAYSHELF_UAM=ON, copy build/dksh/*.dksh to shaders/ "
                        "and update shaders/glsl.sha256")
            endif ()
        endforeach ()
        file(REMOVE_RECURSE ${PLAYSHELF_DKSH_DIR})
        foreach (shader ${PLAYSHELF_SHADERS})
            file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}.dksh DESTINATION ${PLAYSHELF_DKSH_DIR})
        endforeach ()
    endif ()
endif ()
if (PLATFORM_SWITCH)
    if (USE_DEKO3D)
        message(STATUS "PlayShelf renderer: deko3d")
    else ()
        message(STATUS "PlayShelf renderer: OpenGL (mesa)")
    endif ()
endif ()
string(TOUPPER "${CMAKE_BUILD_TYPE}" PLAYSHELF_BUILD_TYPE_UC)
message(STATUS "PlayShelf build type: ${CMAKE_BUILD_TYPE}; C++ flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${PLAYSHELF_BUILD_TYPE_UC}}")

# App metadata. The NACP version must match the release tag (CI checks it).
# release-please bumps these three lines (see release-please-config.json).
set(VERSION_MAJOR "0") # x-release-please-major
set(VERSION_MINOR "0") # x-release-please-minor
set(VERSION_ALTER "0") # x-release-please-patch
set(APP_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}")
set(PROJECT_TITLEID "")
set(PROJECT_AUTHOR "JigSawFr")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

# On desktop borealis looks for "<BRLS_RESOURCES_DIR>/resources/" at runtime:
# point it at the merged copy assembled below.
if (PLATFORM_DESKTOP)
    set(BRLS_RESOURCES_DIR ${CMAKE_BINARY_DIR})
endif ()

add_subdirectory(${BOREALIS_LIBRARY} ${CMAKE_BINARY_DIR}/borealis_library)

find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

# libcurl for the update check (util/http.cpp). On the Switch: devkitPro's
# switch-curl portlib (TLS through the console's ssl service), static, so its
# own dependencies are listed after it. zstd for the .ncz contents
# (install/content_stream.cpp): the switch-zstd portlib, the system's
# libzstd on a desktop.
if (PLATFORM_SWITCH)
    list(APPEND APP_PLATFORM_LIB curl zstd z nx)
else ()
    find_package(CURL REQUIRED)
    find_package(ZLIB REQUIRED)   # util/zip_read.cpp (the Switch links z above)
    find_library(ZSTD_LIBRARY zstd REQUIRED)
    find_path(ZSTD_INCLUDE_DIR zstd.h REQUIRED)
    list(APPEND APP_PLATFORM_LIB CURL::libcurl ZLIB::ZLIB ${ZSTD_LIBRARY})
    list(APPEND APP_PLATFORM_INCLUDE ${ZSTD_INCLUDE_DIR})
endif ()

# Sources: UI (C++) + service layer. The real service layer (source/core/*.c)
# talks to libnx and is Switch-only; the desktop build swaps in the simulated
# backend from source/kit/sim/ so the UI can be run and screenshotted on a PC.
file(GLOB_RECURSE PLAYSHELF_SRC CONFIGURE_DEPENDS source/*.cpp source/*.c)
if (PLATFORM_SWITCH)
    list(FILTER PLAYSHELF_SRC EXCLUDE REGEX ".*/source/(kit/)?sim/.*")
    list(APPEND PLAYSHELF_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
else ()
    list(FILTER PLAYSHELF_SRC EXCLUDE REGEX ".*/source/(kit/)?core/.*\\.c$")
    # The console-free parts are shared with the simulator: one implementation.
    list(APPEND PLAYSHELF_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/kit/core/calendar.c
                              ${CMAKE_CURRENT_SOURCE_DIR}/source/kit/core/write_guard.c)
endif ()

# QR codes (sign-in with GitHub, funding links in About) (MIT, vendored: extern/qrcodegen).
list(APPEND PLAYSHELF_SRC ${CMAKE_CURRENT_SOURCE_DIR}/extern/qrcodegen/qrcodegen.cpp)

program_target(${PROJECT_NAME} "${PLAYSHELF_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_include_directories(${PROJECT_NAME} PRIVATE source source/kit extern/qrcodegen ${APP_PLATFORM_INCLUDE})
# -ffunction-sections / -fdata-sections come from borealis (PUBLIC) and
# --gc-sections from libnx's switch.specs.
# Warnings on PlayShelf's own sources (not borealis, not the vendored
# qrcodegen and stb); CI turns them into errors with -DPLAYSHELF_WERROR=ON.
option(PLAYSHELF_WERROR "Treat PlayShelf's compiler warnings as errors" OFF)
# Also a switch case falling into the next unmarked, a class with virtual
# functions whose destructor is not virtual (C++ only), and a name that hides
# another one (a local, a parameter, a lambda's or a member).
set(PLAYSHELF_WARNINGS -Wall -Wextra -Wimplicit-fallthrough -Wshadow $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>)
if (PLAYSHELF_WERROR)
    list(APPEND PLAYSHELF_WARNINGS -Werror)
endif ()
target_compile_options(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_OPTION} ${PLAYSHELF_WARNINGS})
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/extern/qrcodegen/qrcodegen.cpp PROPERTIES COMPILE_OPTIONS "-w")
# The commit built (at configure time): development builds are told apart by
# it (util/dev_builds.hpp). PLAYSHELF_COMMIT (CI: a pull request's head)
# wins over the checkout's. Empty outside a git checkout.
find_package(Git QUIET)
set(APP_COMMIT "")
if (DEFINED ENV{PLAYSHELF_COMMIT} AND NOT "$ENV{PLAYSHELF_COMMIT}" STREQUAL "")
    string(SUBSTRING "$ENV{PLAYSHELF_COMMIT}" 0 7 APP_COMMIT)
elseif (GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE APP_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
message(STATUS "PlayShelf commit: ${APP_COMMIT}")
# The pull request built (CI: PLAYSHELF_PR, its number), 0 for any other build:
# the change history says which pull request's build made a change.
set(APP_PR 0)
if ("$ENV{PLAYSHELF_PR}" MATCHES "^[1-9][0-9]*$")
    set(APP_PR "$ENV{PLAYSHELF_PR}")
endif ()
# A published release (CI: the build release-please.yml calls for a vX.Y.Z tag,
# PLAYSHELF_RELEASE=1). Anything else is a development build: the version
# badge in the header (view/version_badge.hpp) says so, with the commit.
set(APP_RELEASE 0)
if ("$ENV{PLAYSHELF_RELEASE}" STREQUAL "1")
    set(APP_RELEASE 1)
endif ()
message(STATUS "PlayShelf release build: ${APP_RELEASE}")
# On the two files that read them only: on every other file's command line
# the commit (and the pull request) would change it at each build, and ccache
# would have to preprocess each file again before finding it already compiled
# (the desktop's precompiled header, below, would be built anew every time).
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/source/app.cpp APPEND PROPERTY COMPILE_DEFINITIONS
             APP_VERSION="${APP_VERSION}" APP_COMMIT="${APP_COMMIT}" APP_PR=${APP_PR} APP_RELEASE=${APP_RELEASE})
target_link_libraries(${PROJECT_NAME} PRIVATE borealis ${APP_PLATFORM_LIB})
# On the desktop: borealis, fmt and the standard headers most files include,
# parsed once (a precompiled header) instead of in each of the 200-odd C++
# files: a change to a header many include (ui/ui.hpp, app.hpp) rebuilds in
# about half the time. The Switch and the web keep compiling as before (their
# toolchains, and the release's reproducibility check, are left alone); a
# file that forgets an #include the header gives the desktop fails there.
# qrcodegen is not ours (-w).
if (PLATFORM_DESKTOP AND NOT CMAKE_VERSION VERSION_LESS 3.16)
    target_precompile_headers(${PROJECT_NAME} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:<borealis.hpp$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<fmt/format.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<algorithm$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<chrono$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<functional$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<memory$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>")
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/extern/qrcodegen/qrcodegen.cpp
                                PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif ()
# borealis' headers as system headers: their warnings are not PlayShelf's.
get_target_property(BOREALIS_INCLUDES borealis INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${BOREALIS_INCLUDES})
# On the Switch: a linker map (build/borealis.map) to see what makes the .nro big.
target_link_options(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_LINK_OPTION})
# On the Switch: borealis' userAppInit, which libnx calls before main, timed
# for the diagnostic report's start-up section (source/core/startup_hook.c).
if (PLATFORM_SWITCH)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--wrap=userAppInit)
endif ()

# Resources: borealis built-ins first, then ours on top. On the Switch
# borealis' bundled fonts and demo assets are dropped (the system shared font
# is used); ours stay (resources/font: Cadran's figures).
# en-GB reuses the English strings, already in British spelling (borealis looks
# the locale up verbatim), except the quoted system menu labels, spelt as on a
# US console ("Synchronize Clock via Internet").
set(BOREALIS_RESOURCES ${BOREALIS_LIBRARY}/../resources)
set(MERGED_RESOURCES ${CMAKE_BINARY_DIR}/resources)
set(ASSEMBLE_RESOURCES
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${BOREALIS_RESOURCES} ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}/xml
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}/img/pokemon
    COMMAND ${CMAKE_COMMAND} -E remove -f ${MERGED_RESOURCES}/img/tiles.png ${MERGED_RESOURCES}/img/demo_icon.jpg
            ${MERGED_RESOURCES}/img/aurore_96.png ${MERGED_RESOURCES}/img/aurore_256.png
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES}/i18n/en-US ${MERGED_RESOURCES}/i18n/en-GB
    # About shows the latest releases from it (util/changelog.cpp).
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md ${MERGED_RESOURCES}/CHANGELOG.md
    # ... and the highlights of the running version from its summary
    # (view/release_notes.cpp), the full list a press away.
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/.github/release-notes ${MERGED_RESOURCES}/release-notes
    COMMAND ${CMAKE_COMMAND} -E remove -f ${MERGED_RESOURCES}/release-notes/README.md ${MERGED_RESOURCES}/img/emoji/README.md
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_ICON} ${MERGED_RESOURCES}/img/app_icon.jpg)

set(PLAYSHELF_COPY_SHADERS)
if (PLATFORM_SWITCH AND USE_DEKO3D)
    set(PLAYSHELF_COPY_SHADERS COMMAND ${CMAKE_COMMAND} -E copy_directory ${PLAYSHELF_DKSH_DIR} ${MERGED_RESOURCES}/shaders)
endif ()

if (PLATFORM_SWITCH)
    add_custom_target(${PROJECT_NAME}.nro ALL DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "PlayShelf" "${PROJECT_AUTHOR}"
                "${APP_VERSION}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
        ${ASSEMBLE_RESOURCES}
        ${PLAYSHELF_COPY_SHADERS}
        COMMAND ${CMAKE_COMMAND} -E remove -f ${MERGED_RESOURCES}/font/switch_font.ttf ${MERGED_RESOURCES}/font/switch_icons.ttf
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro
                --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp
                --romfsdir=${MERGED_RESOURCES}
    )
else ()
    add_custom_target(${PROJECT_NAME}_resources ALL ${ASSEMBLE_RESOURCES})
    add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_resources)
endif ()

