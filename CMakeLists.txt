cmake_minimum_required(VERSION 3.10)

# Aurore (JigSawFr/Aurore, main) lives in library/aurore as a git submodule:
# xfangfang/borealis renamed, with our fixes and some from its other forks.
set(AURORE_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/library/aurore/library)

# On the Switch, draw with deko3d (devkitPro's own GPU API) rather than mesa's
# OpenGL: mesa (libEGL + libglapi) was ~5 MB of the 9.4 MB .nro and most of
# the start-up time. -DAURORE_GRAPHICS=GL3 (make GL=1) builds the OpenGL
# variant. Set before Aurore's options so this default wins.
if (AURORE_PLATFORM STREQUAL "SWITCH" AND NOT AURORE_GRAPHICS)
    set(AURORE_GRAPHICS DEKO3D CACHE STRING "" FORCE)
endif ()

# The web simulator (web/README.md): the desktop build, compiled by
# Emscripten for a browser, with SDL3 and OpenGL ES 3 (WebGL 2) from
# Emscripten's own ports instead of the system's. Its toolchain file
# (emcmake) is only read by project(), below.
set(PLAYSHELF_WEB OFF)
if (CMAKE_TOOLCHAIN_FILE MATCHES "Emscripten\\.cmake$")
    set(PLAYSHELF_WEB ON)
endif ()
if (PLAYSHELF_WEB)
    set(AURORE_PLATFORM DESKTOP CACHE STRING "" FORCE)
    set(AURORE_WINDOW SDL3 CACHE STRING "" FORCE)
    set(USE_SYSTEM_SDL3 ON CACHE BOOL "" FORCE)
    set(AURORE_GRAPHICS GLES3 CACHE STRING "" FORCE)
    set(SDL3_DIR ${CMAKE_CURRENT_SOURCE_DIR}/web/cmake)
    add_compile_options(-pthread -sUSE_SDL=3 -sUSE_ZLIB=1 -fexceptions)
endif ()

# The desktop build is the simulator, built in Release: Aurore's debugging
# aids still read from the environment there (AURORE_INSPECTOR,
# AURORE_HOT_RELOAD…; docs/environment.md in Aurore). -DAURORE_DEV_TOOLS=AUTO
# leaves them out of a release build again.
# Its window is SDL3's, Aurore's default on the desktop
# (JigSawFr/Aurore#92); -DAURORE_WINDOW=GLFW builds the GLFW one.
if (AURORE_PLATFORM STREQUAL "DESKTOP" AND NOT PLAYSHELF_WEB)
    set(AURORE_DEV_TOOLS ON CACHE STRING "")
endif ()

# AURORE_PLATFORM / AURORE_GRAPHICS … options, and the PLATFORM_* and USE_*
# variables they set.
include(${AURORE_LIBRARY}/cmake/commonOption.cmake)

# Sets up the devkitPro cross-toolchain when AURORE_PLATFORM=SWITCH.
include(${AURORE_LIBRARY}/cmake/toolchain.cmake)

project(playshelf LANGUAGES CXX C)

if (PLATFORM_SWITCH)
    if (USE_DEKO3D)
        message(STATUS "PlayShelf renderer: deko3d")
    else ()
        message(STATUS "PlayShelf renderer: OpenGL (mesa)")
    endif ()
endif ()
# devkitPro's platform file defaults to Release (-O2 -g; elf2nro keeps no
# debug data). Aurore prints its "Build Type:" line before project() picks
# that default, so say what is actually used.
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

# On desktop Aurore looks for "<AURORE_RESOURCES_ROOT>/resources/" at runtime:
# point it at the merged copy assembled below.
if (PLAYSHELF_WEB)
    # Packaged into the page's file system at /app/resources (the web target's
    # --preload-file, below).
    set(AURORE_RESOURCES_ROOT /app)
elseif (PLATFORM_DESKTOP)
    set(AURORE_RESOURCES_ROOT ${CMAKE_BINARY_DIR})
endif ()

# Aurore links stdc++fs under clang, which Emscripten's libc++ has no
# need of: an empty stand-in.
if (PLAYSHELF_WEB)
    add_library(stdc++fs INTERFACE)
endif ()

add_subdirectory(${AURORE_LIBRARY} ${CMAKE_BINARY_DIR}/aurore)

# deko3d draws with nanovg's shaders, which Aurore compiles with uam (part of
# switch-dev) and builds into the library (<build>/aurore/dksh/).
# Without uam it only warns and looks for them in the romfs, where PlayShelf
# puts none: the app would stop at its first frame, so stop here instead.
if (PLATFORM_SWITCH AND USE_DEKO3D)
    file(READ ${CMAKE_BINARY_DIR}/aurore/dksh/dk_shaders.h PLAYSHELF_DKSH_HEADER)
    if (PLAYSHELF_DKSH_HEADER MATCHES "nullptr")
        message(FATAL_ERROR "Aurore could not compile the deko3d shaders: install uam (dkp-pacman -S uam)")
    endif ()
endif ()

find_package(Threads REQUIRED)
list(APPEND PLAYSHELF_LIBS ${CMAKE_THREAD_LIBS_INIT})

# libcurl for the update check (util/http.cpp). On the Switch: devkitPro's
# switch-curl portlib (TLS through the console's ssl service), static, so its
# own dependencies are listed after it. zstd for the .ncz contents
# (install/content_stream.cpp): the switch-zstd portlib, the system's
# libzstd on a desktop. libwebp for the title database's icons
# (action/library_icons.cpp): switch-libwebp, the system's libwebp.
if (PLATFORM_SWITCH)
    list(APPEND PLAYSHELF_LIBS curl zstd webp z nx)
elseif (PLAYSHELF_WEB)
    # No libcurl in a browser (source/sim/web_http.cpp answers instead);
    # zstd and libwebp, which Emscripten has no port of, built from their
    # releases' decoders (the app only reads .ncz contents and WebP icons).
    include(FetchContent)
    FetchContent_Declare(zstd
        URL https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
        URL_HASH SHA256=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3)
    FetchContent_GetProperties(zstd)
    if (NOT zstd_POPULATED)
        FetchContent_Populate(zstd)
    endif ()
    file(GLOB PLAYSHELF_ZSTD_SRC ${zstd_SOURCE_DIR}/lib/common/*.c ${zstd_SOURCE_DIR}/lib/decompress/*.c)
    add_library(playshelf_zstd STATIC ${PLAYSHELF_ZSTD_SRC})
    target_include_directories(playshelf_zstd PUBLIC ${zstd_SOURCE_DIR}/lib)
    target_compile_definitions(playshelf_zstd PRIVATE ZSTD_DISABLE_ASM ZSTD_MULTITHREAD=0)
    target_compile_options(playshelf_zstd PRIVATE -w)
    FetchContent_Declare(libwebp
        URL https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.5.0.tar.gz
        URL_HASH SHA256=7d6fab70cf844bf6769077bd5d7a74893f8ffd4dfb42861745750c63c2a5c92c)
    FetchContent_GetProperties(libwebp)
    if (NOT libwebp_POPULATED)
        FetchContent_Populate(libwebp)
    endif ()
    file(GLOB PLAYSHELF_WEBP_SRC ${libwebp_SOURCE_DIR}/src/dec/*.c ${libwebp_SOURCE_DIR}/src/dsp/*.c
         ${libwebp_SOURCE_DIR}/src/utils/*.c)
    add_library(playshelf_webp STATIC ${PLAYSHELF_WEBP_SRC})
    target_include_directories(playshelf_webp PUBLIC ${libwebp_SOURCE_DIR}/src PRIVATE ${libwebp_SOURCE_DIR})
    target_compile_options(playshelf_webp PRIVATE -w)
    list(APPEND PLAYSHELF_LIBS playshelf_zstd playshelf_webp)
else ()
    find_package(CURL REQUIRED)
    find_package(ZLIB REQUIRED)   # util/zip_read.cpp (the Switch links z above)
    find_library(ZSTD_LIBRARY zstd REQUIRED)
    find_path(ZSTD_INCLUDE_DIR zstd.h REQUIRED)
    find_library(WEBP_LIBRARY webp REQUIRED)
    find_path(WEBP_INCLUDE_DIR webp/decode.h REQUIRED)
    list(APPEND PLAYSHELF_LIBS CURL::libcurl ZLIB::ZLIB ${ZSTD_LIBRARY} ${WEBP_LIBRARY})
    list(APPEND PLAYSHELF_INCLUDES ${ZSTD_INCLUDE_DIR} ${WEBP_INCLUDE_DIR})
endif ()

# Sources: UI (C++) + service layer. The real service layer (source/core/*.c)
# talks to libnx and is Switch-only; the desktop build swaps in the simulated
# backend from source/kit/sim/ so the UI can be run and screenshotted on a PC.
file(GLOB_RECURSE PLAYSHELF_SRC CONFIGURE_DEPENDS source/*.cpp source/*.c)
if (PLATFORM_SWITCH)
    list(FILTER PLAYSHELF_SRC EXCLUDE REGEX ".*/source/(kit/)?sim/.*")
    list(APPEND PLAYSHELF_SRC ${AURORE_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
else ()
    list(FILTER PLAYSHELF_SRC EXCLUDE REGEX ".*/source/(kit/)?core/.*\\.c$")
    if (PLAYSHELF_WEB)
        list(FILTER PLAYSHELF_SRC EXCLUDE REGEX ".*/source/kit/util/http\\.cpp$")
    else ()
        list(FILTER PLAYSHELF_SRC EXCLUDE REGEX ".*/source/sim/web_.*")
    endif ()
    # The console-free parts are shared with the simulator: one implementation.
    list(APPEND PLAYSHELF_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/kit/core/calendar.c
                              ${CMAKE_CURRENT_SOURCE_DIR}/source/kit/core/write_guard.c)
endif ()

# What the Boréale homebrews share (JigSawFr/boreale-link, a submodule in
# library/boreale-link): the registry of the homebrews installed, the
# shared bell's files, the games' icons and names and the look they share,
# the flat files and the JSON they are written in.
set(BOREALE_LINK ${CMAKE_CURRENT_SOURCE_DIR}/library/boreale-link/source)
list(APPEND PLAYSHELF_SRC ${BOREALE_LINK}/bl_kv.c ${BOREALE_LINK}/bl_app.c ${BOREALE_LINK}/bl_json.c
                          ${BOREALE_LINK}/bl_jread.c ${BOREALE_LINK}/bl_notices.c ${BOREALE_LINK}/bl_updates.c
                          ${BOREALE_LINK}/bl_cache.c ${BOREALE_LINK}/bl_prefs.c)

# QR codes (sign-in with GitHub, funding links in About) (MIT, vendored: extern/qrcodegen).
list(APPEND PLAYSHELF_SRC ${CMAKE_CURRENT_SOURCE_DIR}/extern/qrcodegen/qrcodegen.cpp)

program_target(${PROJECT_NAME} "${PLAYSHELF_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)

target_include_directories(${PROJECT_NAME} PRIVATE source source/kit extern/qrcodegen library/boreale-link/source ${PLAYSHELF_INCLUDES})
# -ffunction-sections / -fdata-sections come from Aurore (PUBLIC) and
# --gc-sections from libnx's switch.specs.
# Warnings on PlayShelf's own sources (not Aurore, not the vendored
# qrcodegen and stb); CI turns them into errors with -DPLAYSHELF_WERROR=ON.
option(PLAYSHELF_WERROR "Treat PlayShelf's compiler warnings as errors" OFF)
# Also a switch case falling into the next unmarked, a class with virtual
# functions whose destructor is not virtual (C++ only), and a name that hides
# another one (a local, a parameter, a lambda's or a member).
set(PLAYSHELF_WARNINGS -Wall -Wextra -Wimplicit-fallthrough -Wshadow $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>)
if (PLAYSHELF_WERROR)
    list(APPEND PLAYSHELF_WARNINGS -Werror)
endif ()
target_compile_options(${PROJECT_NAME} PRIVATE ${PLAYSHELF_WARNINGS})
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
target_link_libraries(${PROJECT_NAME} PRIVATE Aurore::aurore ${PLAYSHELF_LIBS})
# On the desktop: Aurore and the standard headers most files include,
# parsed once (a precompiled header) instead of in each of the 200-odd C++
# files: a change to a header many include (ui/ui.hpp, app.hpp) rebuilds in
# about half the time. The Switch and the web keep compiling as before (their
# toolchains, and the release's reproducibility check, are left alone); a
# file that forgets an #include the header gives the desktop fails there.
# qrcodegen is not ours (-w).
if (PLATFORM_DESKTOP AND NOT CMAKE_VERSION VERSION_LESS 3.16)
    target_precompile_headers(${PROJECT_NAME} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:<aurore.hpp$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<format$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<algorithm$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<chrono$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<functional$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<memory$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>")
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/extern/qrcodegen/qrcodegen.cpp
                                PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif ()
# Aurore's headers as system headers: their warnings are not PlayShelf's.
get_target_property(AURORE_INCLUDES Aurore::aurore INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${AURORE_INCLUDES})
# On the Switch: a linker map (build/playshelf.map) to see what makes the .nro big.
if (PLATFORM_SWITCH)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,-Map,${CMAKE_BINARY_DIR}/playshelf.map)
endif ()
# The desktop simulator's demo (docs/contributing/simulator.md): the folder
# `make desktop` makes it in (tools/web_demo.cpp, tools/web_demo_art.py),
# read from there at each start; empty, none.
if (PLATFORM_DESKTOP AND NOT PLAYSHELF_WEB)
    set(PLAYSHELF_DESKTOP_DEMO "" CACHE PATH "Desktop simulator: the demo's files (tools/web_demo.cpp); empty for none")
    target_compile_definitions(${PROJECT_NAME} PRIVATE PLAYSHELF_DEMO_DIR="${PLAYSHELF_DESKTOP_DEMO}")
endif ()
# The web simulator: threads (a pool started with the page: a thread asked
# for later waits for the browser, which a busy main thread never lets it
# have), WebGL 2, the resources and the demo library server packaged
# (playshelf.data), the environment for the page to set the PLAYSHELF_SIM_*
# variables and write the data folder's files before main() (and run
# dependencies to make main() wait for them or the CJK font), IDBFS to keep
# the data folder in the browser, and ccall for what the page calls while
# PlayShelf runs (source/sim/web_page.cpp, web/simulator.js).
if (PLAYSHELF_WEB)
    set(PLAYSHELF_WEB_DEMO ${CMAKE_BINARY_DIR}/demo CACHE PATH "Web simulator: the demo server's files (tools/web_demo.cpp)")
    target_link_options(${PROJECT_NAME} PRIVATE
        -pthread -fexceptions -sUSE_SDL=3 -sUSE_ZLIB=1
        -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3
        # Not -sGROWABLE_ARRAYBUFFERS, which Emscripten suggests for threads
        # with memory growth: WebGL refuses views of a resizable buffer.
        -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=128MB -sSTACK_SIZE=1MB -sDEFAULT_PTHREAD_STACK_SIZE=1MB
        -sPTHREAD_POOL_SIZE=16
        -sEXIT_RUNTIME=0
        -sEXPORTED_RUNTIME_METHODS=ENV,FS,IDBFS,ccall,addRunDependency,removeRunDependency
        -lidbfs.js
        # SHELL: keeps each pair together (CMake drops a repeated option).
        "SHELL:--preload-file ${CMAKE_BINARY_DIR}/resources@/app/resources"
        "SHELL:--preload-file ${PLAYSHELF_WEB_DEMO}@/app/samples/demo")
endif ()
# On the Switch: Aurore's userAppInit, which libnx calls before main, timed
# for the diagnostic report's start-up section (source/core/startup_hook.c).
if (PLATFORM_SWITCH)
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--wrap=userAppInit)
endif ()

# Resources: Aurore built-ins first, then ours on top, without Aurore's demo
# assets. On the Switch Aurore's desktop fonts (Inter, PromptFont's buttons)
# are dropped too (the system shared font is used); ours stay (resources/font:
# Cadran's figures).
# en-GB reuses the English strings, already in British spelling (Aurore looks
# the locale up verbatim), except the quoted system menu labels, spelt as on a
# US console ("Synchronize Clock via Internet").
set(AURORE_RESOURCES ${AURORE_LIBRARY}/../resources)
set(MERGED_RESOURCES ${CMAKE_BINARY_DIR}/resources)
set(ASSEMBLE_RESOURCES
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${AURORE_RESOURCES} ${MERGED_RESOURCES}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}/xml
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERGED_RESOURCES}/img/gallery
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

# The web simulator: Japanese, Korean and Chinese drawn with the characters
# the catalogs use, cut out of Noto Sans CJK (tools/web_cjk_font.py), as the
# console draws them with its system fonts. It is served next to the page, not
# packaged: the page waits for it only in those languages, and fetches it
# after PlayShelf started in the others (web/README.md). Without the font or
# fontTools, the build goes on and those languages show boxes.
if (PLAYSHELF_WEB)
    set(PLAYSHELF_WEB_CJK_FONT "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc" CACHE FILEPATH
        "Web simulator: Noto Sans CJK (a .ttc) to take the CJK font from")
    find_package(Python3 COMPONENTS Interpreter)
    if (Python3_FOUND)
        execute_process(COMMAND ${Python3_EXECUTABLE} -c "import fontTools" RESULT_VARIABLE PLAYSHELF_NO_FONTTOOLS
                        OUTPUT_QUIET ERROR_QUIET)
    endif ()
    if (Python3_FOUND AND NOT PLAYSHELF_NO_FONTTOOLS AND EXISTS "${PLAYSHELF_WEB_CJK_FONT}")
        set(PLAYSHELF_CJK_CATALOGS)
        foreach (resources ${PROJECT_RESOURCES} ${AURORE_RESOURCES})
            file(GLOB catalogs ${resources}/i18n/ja/*.json ${resources}/i18n/ko/*.json
                 ${resources}/i18n/zh-Hans/*.json ${resources}/i18n/zh-Hant/*.json)
            list(APPEND PLAYSHELF_CJK_CATALOGS ${catalogs})
        endforeach ()
        add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/cjk.otf
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/web_cjk_font.py
                    ${PLAYSHELF_WEB_CJK_FONT} ${CMAKE_BINARY_DIR}/cjk.otf ${CMAKE_CURRENT_SOURCE_DIR}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/web_cjk_font.py ${PLAYSHELF_CJK_CATALOGS}
            COMMENT "Cutting the web simulator's CJK font out of ${PLAYSHELF_WEB_CJK_FONT}")
        add_custom_target(${PROJECT_NAME}_cjk DEPENDS ${CMAKE_BINARY_DIR}/cjk.otf)
        set(PLAYSHELF_SITE_CJK COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/cjk.otf ${CMAKE_BINARY_DIR}/simulator/cjk.otf)
        message(STATUS "Web simulator CJK font: from ${PLAYSHELF_WEB_CJK_FONT}")
    else ()
        message(WARNING "Web simulator: no CJK font (needs Python 3 with fontTools and ${PLAYSHELF_WEB_CJK_FONT}): "
                        "Japanese, Korean and Chinese will show boxes")
    endif ()
endif ()
if (PLATFORM_SWITCH)
    add_custom_target(${PROJECT_NAME}.nro ALL DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "PlayShelf" "${PROJECT_AUTHOR}"
                "${APP_VERSION}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
        ${ASSEMBLE_RESOURCES}
        COMMAND ${CMAKE_COMMAND} -E remove -f ${MERGED_RESOURCES}/font/aurore_text.ttf ${MERGED_RESOURCES}/font/aurore_buttons.ttf
                ${MERGED_RESOURCES}/font/OFL-Inter.txt ${MERGED_RESOURCES}/font/OFL-PromptFont.txt
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro
                --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp
                --romfsdir=${MERGED_RESOURCES}
    )
else ()
    add_custom_target(${PROJECT_NAME}_resources ALL ${ASSEMBLE_RESOURCES})
    add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_resources)
endif ()

# The web simulator's site, ready to serve (web/README.md): the page from
# web/, PlayShelf and its packaged resources, in <build>/simulator/.
if (PLAYSHELF_WEB)
    set(PLAYSHELF_SITE ${CMAKE_BINARY_DIR}/simulator)
    # The header's look and menus (boreale.css, boreale.js), the fonts it
    # names (fonts/: Sora, Figtree, JetBrains Mono, the flags, their
    # licences) and the marks of Boréale's sites: boreale-site's, the
    # website's too (library/boreale-site).
    set(BOREALE_SITE_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/library/boreale-site/assets)
    add_custom_target(${PROJECT_NAME}_simulator ALL DEPENDS ${PROJECT_NAME}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${PLAYSHELF_SITE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PLAYSHELF_SITE}
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html ${CMAKE_CURRENT_SOURCE_DIR}/web/theme.js ${CMAKE_CURRENT_SOURCE_DIR}/web/sim_logic.js
                ${CMAKE_CURRENT_SOURCE_DIR}/web/simulator.js
                ${CMAKE_CURRENT_SOURCE_DIR}/web/simulator.css ${CMAKE_CURRENT_SOURCE_DIR}/web/params.json
                ${CMAKE_CURRENT_SOURCE_DIR}/web/sw.js ${CMAKE_CURRENT_SOURCE_DIR}/web/manifest.webmanifest ${PROJECT_ICON}
                ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.js ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.wasm
                ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.data
                ${CMAKE_CURRENT_SOURCE_DIR}/images/apple-touch-icon.png
                ${CMAKE_CURRENT_SOURCE_DIR}/images/icon-192.png ${CMAKE_CURRENT_SOURCE_DIR}/images/icon-512.png
                ${PLAYSHELF_SITE}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/branding/icon.svg ${PLAYSHELF_SITE}/icon.svg
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/branding/mark.svg ${PLAYSHELF_SITE}/mark.svg
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${BOREALE_SITE_ASSETS}/marks ${PLAYSHELF_SITE}/marks
        COMMAND ${CMAKE_COMMAND} -E copy ${BOREALE_SITE_ASSETS}/boreale.css ${BOREALE_SITE_ASSETS}/boreale.js ${PLAYSHELF_SITE}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${BOREALE_SITE_ASSETS}/fonts ${PLAYSHELF_SITE}/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/web/i18n ${PLAYSHELF_SITE}/i18n
        ${PLAYSHELF_SITE_CJK}
        COMMENT "Assembling the web simulator's site in ${PLAYSHELF_SITE}")
    if (TARGET ${PROJECT_NAME}_cjk)
        add_dependencies(${PROJECT_NAME}_simulator ${PROJECT_NAME}_cjk)
    endif ()
endif ()

