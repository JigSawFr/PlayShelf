# Thin shim over CMake — the real build is in CMakeLists.txt.
#
#   make              -> ./playshelf.nro            (needs devkitPro, DEVKITPRO set)
#   make dist         -> ./playshelf.zip            (unzip onto the SD card root)
#   make desktop      -> ./build-desktop/playshelf  (UI with a simulated backend and the demo; SDL3, built with it,
#                        and Playwright for the demo's pictures: DESKTOP_DEMO= builds it without)
#   make web          -> ./build-web/simulator/     (the web simulator; needs Emscripten's emcmake)
#   make web-serve    -> make web, served at http://localhost:8000
#   make test         -> host unit tests (plain gcc/g++, ASan + UBSan + libstdc++'s assertions; parallel, all suites)
#   make check        -> make test, then the resource and source checks and compat.json
#   make fuzz         -> libFuzzer runs of the parsers (clang; FUZZ_SECONDS=30 each, parallel)
#   make nxlink       -> push to a Switch running hbmenu (press Y there first)
#
# GL=1 builds the Switch .nro with OpenGL (mesa) instead of deko3d.
# JOBS=n sets the parallel build (and host test) jobs (default: every core).
# SAN= turns the sanitizers of the host tests off (e.g. a compiler without
# them). COV=--coverage builds them for gcov (CI: with gcc and SAN=, summarised
# by gcovr).
# SOURCE_DATE_EPOCH (default: the last commit's time) dates every file of the
# release zips, so the same commit gives the same zip.
# CMAKE_C_COMPILER_LAUNCHER / CMAKE_CXX_COMPILER_LAUNCHER=ccache in the
# environment are picked up by CMake (CI uses them). CMAKE_ARGS=... is passed
# to the configure step of `make` (so `make dist`) and `make desktop` (CI
# builds both with CMAKE_ARGS=-DPLAYSHELF_WERROR=ON, warnings as errors).

TARGET  := playshelf
BUILD   := build
# The kit (library/boreale-kit, a submodule): its sources are built by the suites that need them; its own suites run in its repository.
KIT     := library/boreale-kit/source
DESKTOP := build-desktop
WEB     := build-web
CC      ?= gcc
CXX     ?= g++
TESTOUT := $(BUILD)/host-tests
JOBS    ?= $(shell nproc 2>/dev/null || echo 4)
SAN     ?= -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined
COV     ?=
# -Wshadow as the app is built (CMakeLists.txt); -Itests: tests/check.h, the
# CHECK() every suite uses. _GLIBCXX_ASSERTIONS: libstdc++ checks what the
# sanitizers cannot see, an index within a vector's capacity but past its
# size, front() of an empty container, a string_view cut past its end
# (the fuzz targets below take it too).
CWARN   := -Wall -Wextra -Wshadow -Werror -D_GLIBCXX_ASSERTIONS $(SAN) $(COV) -Itests
SOURCE_DATE_EPOCH ?= $(shell git log -1 --format=%ct 2>/dev/null || echo 0)
# A zip that depends only on the files: one date for all, entries in a fixed
# order, no extra file attributes (-X), and TZ=UTC as a zip keeps local times.
# Run inside the tree, the zip file last.
ZIP_TREE = find . -exec touch -h -d @$(SOURCE_DATE_EPOCH) {} + && find . -mindepth 1 | LC_ALL=C sort | TZ=UTC zip -X -@
CMAKE_ARGS ?=
JSON_HPP := library/aurore/library/include/aurore/extern/nlohmann/json.hpp

.PHONY: all clean dist nxlink desktop web web-serve site test fuzz check FORCE

RENDERER := $(if $(GL),-DAURORE_GRAPHICS=GL3,-DAURORE_GRAPHICS=DEKO3D)

all:
	@cmake -B $(BUILD) -S . -DAURORE_PLATFORM=SWITCH $(RENDERER) $(CMAKE_ARGS)
	@cmake --build $(BUILD) --target $(TARGET).nro -j $(JOBS)
	@cp $(BUILD)/$(TARGET).nro  $(TARGET).nro
	@cp $(BUILD)/$(TARGET).nacp $(TARGET).nacp

# Layout on the SD card: one folder per app (hbmenu / hb-appstore / sphaira
# convention) + the sphaira GitHub-updater entry. The GPL text travels with
# the binary.
dist: all
	@echo making dist ...
	@rm -rf out/ $(TARGET).zip
	@mkdir -p out/switch/$(TARGET) out/config/sphaira/github
	@cp $(BUILD)/$(TARGET).nro out/switch/$(TARGET)/
	@cp LICENSE out/switch/$(TARGET)/LICENSE.txt
	@cp packaging/sphaira/$(TARGET).json out/config/sphaira/github/
	@cd out && $(ZIP_TREE) ../$(TARGET).zip

# The simulators' demo (web/README.md, docs/contributing/simulator.md): its
# library server and the other services' answers about its made-up games,
# made on the host by tools/web_demo.cpp (the install suites' package
# builders, with libzstd and zlib) and tools/web_demo_art.py (its pictures:
# SVG scenes rendered in Chromium through Playwright, saved with Pillow, by
# ART_PYTHON), into <build>/demo/: packaged with the web simulator, read from
# there by the desktop one.
WEB_DEMO_SOURCES := tools/web_demo.cpp $(KIT)/util/aes.cpp source/install/rsa.cpp source/install/pfs.cpp \
                source/install/cnmt.cpp source/install/nca.cpp $(KIT)/util/sha256.cpp
%/web_demo: $(WEB_DEMO_SOURCES) $(wildcard tests/install/*.hpp)
	@mkdir -p $*
	$(CXX) -std=c++23 -O1 -Isource -Isource/kit -I$(KIT) -Itests -Itests/install -isystem library/aurore/library/include \
	    -Wall -Wextra -Wshadow -Werror $(WEB_DEMO_SOURCES) -lzstd -lz -o $@
ART_PYTHON ?= python3
%/demo/lantern/apps.json: %/web_demo tools/web_demo_games.json tools/web_demo_art.py tools/web_demo_scenes.py
	@rm -rf $*/demo
	@$*/web_demo $*/demo tools/web_demo_games.json
	@$(ART_PYTHON) tools/web_demo_art.py tools/web_demo_games.json $*/demo || { rm -rf $*/demo; exit 1; }
.PRECIOUS: %/web_demo %/demo/lantern/apps.json

# Then PlayShelf with Emscripten.
web: $(WEB)/demo/lantern/apps.json
	@emcmake cmake -B $(WEB) -S . -DCMAKE_BUILD_TYPE=Release -DPLAYSHELF_WEB_DEMO=$(abspath $(WEB))/demo $(CMAKE_ARGS)
	@"$$(command -v cmake)" --build $(WEB) -j $(JOBS)

web-serve: web
	@python3 web/serve.py $(WEB)/simulator 8000

# The website (site/README.md), the simulator in it when built: served at
# http://localhost:8000/.
site:
	@rm -rf build-site
	@SITE_SIMULATOR=$(WEB)/simulator python3 tools/build_site.py build-site .
	@python3 tools/check_site_links.py build-site
	@if [ -d $(WEB)/simulator ]; then cp -r $(WEB)/simulator build-site/simulator; fi
	@python3 web/serve.py build-site 8000

# The desktop simulator with the demo (DESKTOP_DEMO=1, the default; empty:
# without it, and without Playwright).
DESKTOP_DEMO ?= 1
desktop: $(if $(DESKTOP_DEMO),$(DESKTOP)/demo/lantern/apps.json)
	@cmake -B $(DESKTOP) -S . -DAURORE_PLATFORM=DESKTOP -DCMAKE_BUILD_TYPE=Release \
	    -DPLAYSHELF_DESKTOP_DEMO=$(if $(DESKTOP_DEMO),$(abspath $(DESKTOP))/demo,) $(CMAKE_ARGS)
	@cmake --build $(DESKTOP) -j $(JOBS)

# Host tests: one program per suite, built straight from its sources (no
# object files, so gcov's .gcno/.gcda sit next to the program in
# build/host-tests/). A suite is three variables: <name>_SRC (its sources,
# the test last), <name>_FLAGS (the standard, defines, include paths; a C++
# suite without one gets TEST_CXXFLAGS) and an optional <name>_LIBS; the test
# source's suffix picks the compiler.
# `make test` builds and runs every suite in parallel (JOBS) and keeps going
# when one fails, so a run reports every failing suite, not the first: each
# suite's output is in build/host-tests/<name>.log, and <name>.ok marks a
# passed one (CI reads both for the job summary).

# C: the kit's console-free C (source/kit/core, source/kit/util).

# C++: the UI-free helpers and flow logic (source/kit/util, source/kit/action,
# source/util), by default with TEST_CXXFLAGS (a <name>_FLAGS only where a
# suite differs). Aurore's headers (nlohmann/json) as system headers: their
# warnings are not PlayShelf's (clang 22 deprecates json.hpp's literal
# operators, under -Werror). util/date stands on core/calendar.c: a suite
# that links it builds that C file too (its <name>_CSRC, without having to
# say so).
TEST_CXXFLAGS   := -std=c++23 -Isource -Isource/kit -I$(KIT) -Ilibrary/boreale-link/source -isystem library/aurore/library/include
CALENDAR_C      := $(KIT)/core/calendar.c
CALENDAR_CFLAGS := -std=gnu11 -I$(KIT)/core
K := source/kit
# Aurore's Cadran (library/aurore): the kit's UI helpers, tested here.
C := library/aurore/library/lib/cadran
# A secret sealed for this console: AES and HMAC, and the console's key
# (the desktop build's random one: sim/sim_device_key.cpp).
SECRET_BOX := $(KIT)/util/secret_box.cpp $(KIT)/util/aes.cpp $(KIT)/sim/sim_device_key.cpp
# The Lantern SDK (library/lantern-sdk, a submodule), built once for the host
# suites: its client library as a static archive, no curl or mbedtls adapter
# (the suites script the transport).
LANTERN_HOST := $(BUILD)/lantern-host
LANTERN_LIB  := $(LANTERN_HOST)/liblantern.a
LANTERN_INC  := -isystem library/lantern-sdk/cpp/include
$(LANTERN_LIB):
	@test -f library/lantern-sdk/cpp/CMakeLists.txt || { echo "the Lantern SDK is missing: run git submodule update --init"; exit 1; }
	cmake -S library/lantern-sdk/cpp -B $(LANTERN_HOST) -DCMAKE_BUILD_TYPE=Release -DLANTERN_BUILD_TESTS=OFF -DLANTERN_BUILD_EXAMPLES=OFF -DLANTERN_INSTALL=OFF -DLANTERN_CURL=OFF -DLANTERN_MBEDTLS=OFF > $(LANTERN_HOST).log 2>&1 || { cat $(LANTERN_HOST).log; exit 1; }
	cmake --build $(LANTERN_HOST) --target lantern -j$(JOBS) > $(LANTERN_HOST).log 2>&1 || { tail -n 30 $(LANTERN_HOST).log; exit 1; }
# Lantern credentials kept on the SD card, sealed.
lantern_store_SRC   := source/lantern_app/credential_store.cpp $(SECRET_BOX) $(KIT)/util/sha256.cpp $(KIT)/util/paths.cpp tests/lantern_store/test.cpp
lantern_store_FLAGS := $(TEST_CXXFLAGS) $(LANTERN_INC)
lantern_store_LIBS  := $(LANTERN_LIB)
$(TESTOUT)/lantern_store: $(LANTERN_LIB)
# Lantern's apps and sections as the neutral catalogue.
lantern_catalog_SRC   := source/lantern_app/catalog_map.cpp source/library/catalog.cpp source/library/facts.cpp $(KIT)/util/sha256.cpp tests/lantern_catalog/test.cpp
lantern_catalog_FLAGS := $(TEST_CXXFLAGS) $(LANTERN_INC)
lantern_catalog_LIBS  := $(LANTERN_LIB)
$(TESTOUT)/lantern_catalog: $(LANTERN_LIB)
# A Lantern server read over a scripted network.
lantern_session_SRC   := source/lantern_app/session.cpp source/lantern_app/catalog_map.cpp source/library/catalog.cpp source/library/facts.cpp source/library/server.cpp \
                         source/library/servers.cpp $(KIT)/util/sha256.cpp $(KIT)/util/paths.cpp tests/lantern_session/test.cpp
lantern_session_FLAGS := $(TEST_CXXFLAGS) $(LANTERN_INC)
lantern_session_LIBS  := $(LANTERN_LIB)
$(TESTOUT)/lantern_session: $(LANTERN_LIB)
# A package read by signed ranges, over a scripted network.
lantern_range_SRC   := source/lantern_app/range_source.cpp source/lantern_app/session.cpp source/lantern_app/catalog_map.cpp source/library/catalog.cpp source/library/facts.cpp \
                       source/library/server.cpp source/library/servers.cpp $(KIT)/util/sha256.cpp $(KIT)/util/paths.cpp tests/lantern_range/test.cpp
lantern_range_FLAGS := $(TEST_CXXFLAGS) $(LANTERN_INC)
lantern_range_LIBS  := $(LANTERN_LIB)
$(TESTOUT)/lantern_range: $(LANTERN_LIB)
# A package put together from an older copy and the ranges that differ.
lantern_delta_SRC   := source/lantern_app/delta.cpp source/lantern_app/session.cpp source/lantern_app/catalog_map.cpp source/library/catalog.cpp source/library/facts.cpp source/library/server.cpp source/library/servers.cpp $(KIT)/util/sha256.cpp $(KIT)/util/paths.cpp tests/lantern_delta/test.cpp
lantern_delta_FLAGS := $(TEST_CXXFLAGS) $(LANTERN_INC)
lantern_delta_LIBS  := $(LANTERN_LIB)
$(TESTOUT)/lantern_delta: $(LANTERN_LIB)
# Peers found, and a package read from one, over a scripted network.
lantern_peers_SRC   := source/lantern_app/peers.cpp source/lantern_app/delta.cpp source/lantern_app/session.cpp source/lantern_app/catalog_map.cpp \
                       source/library/catalog.cpp source/library/facts.cpp source/library/server.cpp source/library/servers.cpp \
                       $(KIT)/util/sha256.cpp $(KIT)/util/paths.cpp tests/lantern_peers/test.cpp
lantern_peers_FLAGS := $(TEST_CXXFLAGS) $(LANTERN_INC)
lantern_peers_LIBS  := $(LANTERN_LIB)
$(TESTOUT)/lantern_peers: $(LANTERN_LIB)
# A port's game data looked for on the SD card.
lantern_ports_SRC   := source/lantern_app/port_data.cpp source/library/catalog.cpp tests/lantern_ports/test.cpp
lantern_ports_FLAGS := $(TEST_CXXFLAGS) $(LANTERN_INC)
lantern_ports_LIBS  := $(LANTERN_LIB)
$(TESTOUT)/lantern_ports: $(LANTERN_LIB)
# The system under the Lantern client: random bytes, the time, the discovery socket.
lantern_system_SRC   := source/lantern_app/system.cpp tests/lantern_system/test.cpp
lantern_system_FLAGS := $(TEST_CXXFLAGS) $(LANTERN_INC)
lantern_system_LIBS  := $(LANTERN_LIB)
$(TESTOUT)/lantern_system: $(LANTERN_LIB)
# Servers found on the LAN.
lantern_discovery_SRC   := source/lantern_app/discovery.cpp source/library/catalog.cpp $(KIT)/util/sha256.cpp tests/lantern_discovery/test.cpp
lantern_discovery_FLAGS := $(TEST_CXXFLAGS) $(LANTERN_INC)
lantern_discovery_LIBS  := $(LANTERN_LIB)
$(TESTOUT)/lantern_discovery: $(LANTERN_LIB)

# A homebrew package written onto the SD card, and the book of what is installed.
homebrew_install_SRC   := source/homebrew/install.cpp source/saves/archive.cpp $(KIT)/util/sha256.cpp $(KIT)/util/paths.cpp $(KIT)/util/zip_write.cpp tests/homebrew_install/test.cpp
homebrew_install_FLAGS := $(TEST_CXXFLAGS) $(LANTERN_INC)
homebrew_install_LIBS  := $(LANTERN_LIB) -lz
$(TESTOUT)/homebrew_install: $(LANTERN_LIB)

# A Lantern server's homebrew catalogue as the tab's items.
homebrew_map_SRC   := source/lantern_app/homebrew_map.cpp source/lantern_app/session.cpp source/lantern_app/catalog_map.cpp source/library/catalog.cpp source/library/facts.cpp source/library/server.cpp \
                      source/library/servers.cpp $(KIT)/util/sha256.cpp $(KIT)/util/paths.cpp tests/homebrew_map/test.cpp
homebrew_map_FLAGS := $(TEST_CXXFLAGS) $(LANTERN_INC)
homebrew_map_LIBS  := $(LANTERN_LIB)
$(TESTOUT)/homebrew_map: $(LANTERN_LIB)
# The install formats (source/install/: crypto, PFS0/HFS0, XCI, CNMT, NCA),
# pure C++ with the kit's SHA-256.
INSTALL_FORMATS := $(KIT)/util/aes.cpp source/install/rsa.cpp source/install/pfs.cpp source/install/cnmt.cpp \
                   source/install/nca.cpp $(KIT)/util/sha256.cpp
install_SRC := $(INSTALL_FORMATS) tests/install/test.cpp
# A content (.nca or .ncz) as it arrives, made the bytes of its NCA; zstd
# from the system (libzstd-dev), compression too, for the NCZ it builds.
install_stream_SRC  := $(INSTALL_FORMATS) source/install/content_stream.cpp tests/install_stream/test.cpp
install_stream_LIBS := -lzstd
# The integrity tree of a RomFS section.
ivfc_SRC := source/install/ivfc.cpp $(KIT)/util/sha256.cpp tests/ivfc/test.cpp
# The compression layer of an NCA section (blocks and bucket tree); liblz4 from
# the system (liblz4-dev), the layer's codec is the caller's.
compressed_storage_SRC  := source/install/compressed_storage.cpp tests/compressed_storage/test.cpp
compressed_storage_LIBS := -llz4
# The integrity tree of a RomFS section.
ivfc_SRC := source/install/ivfc.cpp $(KIT)/util/sha256.cpp tests/ivfc/test.cpp
# An update's RomFS section over a base game's (relocation and subsection tables); liblz4 only for the shared builders.
report_qr_SRC  := $(KIT)/util/report_qr.cpp tests/report_qr/test.cpp
report_qr_LIBS := -lzstd
cpu_boost_SRC := source/util/cpu_boost.cpp tests/cpu_boost/test.cpp

bktr_SRC  := $(INSTALL_FORMATS) source/install/bktr.cpp source/install/ivfc.cpp tests/bktr/test.cpp
bktr_LIBS := -llz4
# A Program NCA made over with its RomFS compressed (liblz4 from the system).
recompress_SRC  := $(INSTALL_FORMATS) source/install/compressed_storage.cpp source/install/ivfc.cpp source/install/bktr.cpp source/install/recompress.cpp \
                   tests/recompress/test.cpp
recompress_LIBS := -llz4
# The content meta of a recompressed program, and the .cnmt.nca that holds it.
recompress_meta_SRC  := $(INSTALL_FORMATS) source/install/ivfc.cpp source/install/recompress_meta.cpp tests/recompress_meta/test.cpp
recompress_meta_LIBS := -llz4
# An installed title made over with its program compressed, as a transaction,
# faults and crashes injected everywhere (liblz4 from the system).
compress_title_SRC  := $(INSTALL_FORMATS) source/install/compressed_storage.cpp source/install/ivfc.cpp source/install/bktr.cpp source/install/recompress.cpp \
                       source/install/recompress_meta.cpp source/install/compress_title.cpp tests/compress_title/test.cpp
compress_title_LIBS := -llz4
# A package (NSP, NSZ, XCI) installed as one transaction, on a console in memory.
install_txn_SRC  := $(INSTALL_FORMATS) source/install/content_stream.cpp source/install/package.cpp \
                    source/install/installer.cpp tests/install_txn/test.cpp
install_txn_LIBS := -lzstd
# The library's neutral parts (source/library/: addresses, endpoints, title ids).
LIBRARY_FORMATS := source/library/catalog.cpp source/library/server.cpp
library_SRC := $(LIBRARY_FORMATS) tests/library/test.cpp
# The servers' file: its schema, the carry-over of schema 1.
library_servers_SRC := source/library/servers.cpp source/library/server.cpp $(KIT)/util/paths.cpp tests/library_servers/test.cpp
library_titles_SRC := source/library/catalog.cpp source/library/facts.cpp source/library/titles.cpp tests/library_titles/test.cpp
# What is known of the games beyond their catalogue (Nlib API's), kept on
# the SD card.
library_facts_SRC := source/library/facts.cpp tests/library_facts/test.cpp
# What the notification bell holds: its entries, those put away.
notice_logic_SRC := source/action/notice_logic.cpp tests/notice_logic/test.cpp
# How well known the games are, their trend and critics (popular.json, Atlas).
library_popular_SRC := source/library/popular.cpp tests/library_popular/test.cpp
# When each game was first seen on a server; the wishlist's ids.
library_seen_SRC := source/library/seen.cpp tests/library_seen/test.cpp
# A game's save as a zip archive, and back.
save_archive_SRC  := source/saves/archive.cpp $(KIT)/util/zip_write.cpp $(KIT)/util/paths.cpp tests/save_archive/test.cpp
save_archive_LIBS := -lz
# The cheat files on the SD card: build ids, their paths, those installed.
library_services_SRC := source/library/services.cpp $(KIT)/util/paths.cpp tests/library_services/test.cpp
# What Atlas knows of each game beyond its popularity (games.json,
# similar.json), and the recommendations made from every signal.
library_signals_SRC := source/library/signals.cpp tests/library_signals/test.cpp
# What Atlas' last run changed (changes.json): the games out today, the games new, the trends moved.
library_changes_SRC := source/library/signals.cpp source/library/changes.cpp tests/library_changes/test.cpp
# What the bell tells of Atlas' new games and trends, each once (news.json).
library_news_SRC := source/library/signals.cpp source/library/changes.cpp source/library/seen.cpp \
                    source/library/news.cpp tests/library_news/test.cpp
# Atlas' index.json, and which of its files to download again.
library_index_SRC := source/library/signals.cpp source/library/insights_index.cpp $(KIT)/util/sha256.cpp tests/library_index/test.cpp
library_recommend_SRC := source/library/signals.cpp source/library/popular.cpp source/library/seen.cpp \
                         source/library/recommend.cpp tests/library_recommend/test.cpp
# The console's games by title (base game, update, add-ons) and what a
# catalogue has beyond them.
library_installed_SRC := source/library/catalog.cpp source/library/facts.cpp source/library/titles.cpp source/library/installed.cpp tests/library_installed/test.cpp
# The downloads' queue (source/download/): its file, the journals in it, the
# speed, the placeholders no job keeps.
download_queue_SRC  := $(filter-out tests/%,$(install_txn_SRC)) source/download/queue.cpp $(KIT)/util/paths.cpp \
                       tests/download_queue/test.cpp
download_queue_LIBS := -lzstd
# What a failed download's error means and what can be done about it.
download_recovery_SRC  := $(filter-out tests/%,$(download_queue_SRC)) source/download/recovery.cpp tests/download_recovery/test.cpp
download_recovery_LIBS := -lzstd
# A package's first bytes from a file fetched ahead, the rest from its source.
prefix_source_SRC := source/install/prefix_source.cpp tests/prefix_source/test.cpp
# CyberFoil-DB's packs (names, icons) and their manifest; a title list made a pack.
titledb_SRC := source/titledb/pack.cpp source/titledb/manifest.cpp source/titledb/list.cpp source/titledb/nlib.cpp tests/titledb/test.cpp
# Their folders on the SD card: updates checked by their SHA-256, the sources' order.
titledb_store_SRC := $(filter-out tests/%,$(titledb_SRC)) source/titledb/store.cpp $(KIT)/util/sha256.cpp $(KIT)/util/paths.cpp \
                     tests/titledb_store/test.cpp
# A range over several connections at once, handed on in order.
split_stream_SRC  := source/install/split_stream.cpp tests/split_stream/test.cpp
split_stream_LIBS := -pthread
# The desktop build's console (sim/sim_install.cpp) behind install::Console.
install_sim_SRC  := $(filter-out tests/%,$(install_txn_SRC)) source/install/console_core.cpp source/sim/sim_install.cpp \
                    $(KIT)/util/paths.cpp $(KIT)/sim/sim_env.cpp tests/install_sim/test.cpp
install_sim_CSRC := $(KIT)/sim/sim_sysinfo.c
install_sim_CFLAGS := -std=gnu11 -I$(KIT)/core -Isource
install_sim_LIBS := -lzstd
# The header-only helpers: utf8, emoji, text, json_util.
config_SRC   := $(KIT)/util/paths.cpp $(KIT)/util/date.cpp source/util/config.cpp tests/config/test.cpp
# The download and its resume: without PLAYSHELF_SIM_DEV_BUILDS the desktop
# build takes the console's path, here against a fake http::download and
# http::redirect in the test (-D__SWITCH__ would reach date.cpp's switch.h).
# Aurore's own string tables (core/i18n.cpp), read from other threads while
# the language changes. A copy of the files at the submodule's commit, first
# in the include path, made again when that commit changes (the stamp holds
# it, rewritten only then). C++23, as Aurore is built (its headers use
# defaulted comparisons).
I18N_COPY    := $(TESTOUT)/aurore_i18n
I18N_STAMP   := $(TESTOUT)/aurore_i18n.commit
I18N_COMMIT  := $(shell git -C library/aurore rev-parse HEAD 2>/dev/null)
$(I18N_STAMP): FORCE | $(TESTOUT)
	@echo '$(I18N_COMMIT)' | cmp -s - $@ || echo '$(I18N_COMMIT)' > $@
$(I18N_COPY)/i18n.cpp: $(I18N_STAMP) | $(TESTOUT)
	@rm -rf $(I18N_COPY) && mkdir -p $(I18N_COPY)/library/lib/core $(I18N_COPY)/library/include/aurore/core
	@git -C library/aurore show HEAD:library/lib/core/i18n.cpp > $(I18N_COPY)/library/lib/core/i18n.cpp
	@git -C library/aurore show HEAD:library/include/aurore/core/i18n.hpp > $(I18N_COPY)/library/include/aurore/core/i18n.hpp
	@git -C library/aurore show HEAD:library/include/aurore/core/platform.hpp > $(I18N_COPY)/library/include/aurore/core/platform.hpp
	@# The suffix without a space, as newer clang wants it (-Wdeprecated-literal-operator),
	@# in this copy only: Aurore itself is built as it is.
	@sed 's/operator"" _i18n/operator""_i18n/' $(I18N_COPY)/library/lib/core/i18n.cpp > $@
# With its string and file helpers (core/util.cpp), as it is.
i18n_threads_SRC   := $(I18N_COPY)/i18n.cpp library/aurore/library/lib/core/util.cpp tests/i18n_threads/test.cpp
i18n_threads_FLAGS := -std=c++23 -isystem $(I18N_COPY)/library/include -isystem library/aurore/library/include \
                      $(addprefix -isystem library/aurore/library/,include/aurore/extern include/aurore/extern/nanovg \
                        include/aurore/extern/tinyxml2 lib/extern/tweeny/include lib/extern/yoga) \
                      -DAURORE_RESOURCES='"./"'
i18n_threads_LIBS  := -pthread
# A C source a C++ suite links (clang++ refuses to build a .c as C++).
# The callbacks a store tells of its changes (header only).
# A whole text read as a number (header only).
# What the eShop prices left in the data folder, removed (a migration step).
no_prices_SRC := $(KIT)/util/paths.cpp source/action/notice_logic.cpp source/util/no_prices.cpp tests/no_prices/test.cpp

TESTS := $(sort $(patsubst %_SRC,%,$(filter %_SRC,$(.VARIABLES))))
TEST_PROGRAMS := $(addprefix $(TESTOUT)/,$(TESTS))
TEST_OKS := $(addsuffix .ok,$(TEST_PROGRAMS))
# Every header: a suite is rebuilt when one changes (coarse, but a suite
# compiles in seconds and CI always starts cold).
TEST_HEADERS := $(wildcard tests/*.h tests/*.hpp tests/*/*.h tests/*/*.hpp source/*.hpp source/*/*.h source/*/*.hpp source/kit/*/*.h source/kit/*/*.hpp $(KIT)/*.hpp $(KIT)/*/*.h $(KIT)/*/*.hpp)

$(TESTOUT):
	@mkdir -p $@

# make only compares dates: this file is rewritten when the compiler or the
# flags change (CC=clang, SAN=, COV=--coverage), so the suites are rebuilt.
TEST_FLAGS_FILE := $(TESTOUT)/flags
$(TEST_FLAGS_FILE): FORCE | $(TESTOUT)
	@echo "$(CC) $(CXX) $(CWARN)" | cmp -s - $@ || echo "$(CC) $(CXX) $(CWARN)" > $@
FORCE:

# The suite's program. Second expansion: the pattern's stem names the
# variables above.
.SECONDEXPANSION:
# An optional <name>_CSRC (C files, with <name>_CFLAGS) is built by $(CC)
# into <name>.<file>.o each and linked into a C++ suite; one that links
# util/date.cpp gets CALENDAR_C (with CALENDAR_CFLAGS) when it names none.
test_flags  = $(or $($1_FLAGS),$(if $(filter %.cpp,$($1_SRC)),$(TEST_CXXFLAGS)))
test_csrc   = $(or $($1_CSRC),$(if $(filter $(KIT)/util/date.cpp,$($1_SRC)),$(CALENDAR_C)))
test_cflags = $(if $($1_CSRC),$($1_CFLAGS),$(CALENDAR_CFLAGS))
test_cobj   = $(TESTOUT)/$1.$(basename $(notdir $2)).o
test_cobjs  = $(foreach c,$(call test_csrc,$1),$(call test_cobj,$1,$c))
$(TEST_PROGRAMS): $(TESTOUT)/%: $$($$*_SRC) $$(call test_csrc,$$*) $(TEST_HEADERS) $(TEST_FLAGS_FILE) | $(TESTOUT)
	$(foreach c,$(call test_csrc,$*),$(CC) $(call test_cflags,$*) $(CWARN) -c $c -o $(call test_cobj,$*,$c) &&) true
	$(if $(filter %.cpp,$($*_SRC)),$(CXX),$(CC)) $(call test_flags,$*) $(CWARN) $($*_SRC) $(call test_cobjs,$*) $($*_LIBS) -o $@

# Running it: the output goes to <name>.log and the terminal; <name>.ok only
# when it passed.
$(TEST_OKS): $(TESTOUT)/%.ok: $(TESTOUT)/%
	@rm -f $@
	@$< > $<.log 2>&1; r=$$?; cat $<.log; if [ $$r -eq 0 ]; then touch $@; else echo "$*: exit status $$r" >&2; exit $$r; fi

test:
	@test -f $(JSON_HPP) || { echo "Aurore is missing (nlohmann/json for the tests): run git submodule update --init"; exit 1; }
	@rm -f $(TEST_OKS)
	@$(MAKE) --no-print-directory -k $(if $(findstring -j,$(MAKEFLAGS)),,-j$(JOBS)) -Otarget $(TEST_OKS)

# Fuzzing: one libFuzzer program per parser of outside data (tests/fuzz/,
# see tests/README.md), built by clang with ASan and UBSan, each run for
# FUZZ_SECONDS from the seed corpus in tests/fuzz/<name>/corpus (left as it
# is: what a run finds goes to build/fuzz/<name>.corpus), with the libFuzzer
# dictionary tests/fuzz/<name>/dict when there is one. In parallel like
# `make test`, all of them even when one fails; a crash leaves its input as
# build/fuzz/<name>-crash-* (replay it: build/fuzz/<name> <that file>). A
# target is <name>_FUZZ (its sources, the fuzz target last; a .c one makes
# it a C program), an optional <name>_FUZZ_CSRC (one C source a C++ target
# links), <name>_FUZZ_FLAGS and <name>_FUZZ_LIBS. A parser a suite tests
# takes that suite's sources: $(filter-out tests/%,$(<suite>_SRC)).
FUZZOUT      := $(BUILD)/fuzz
FUZZ_SECONDS ?= 30
FUZZ_CC      ?= clang
FUZZ_CXX     ?= clang++
FUZZ_SAN     := -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g -O1
FUZZ_CFLAGS  := -Wall -Wextra -Wshadow -Werror -D_GLIBCXX_ASSERTIONS $(FUZZ_SAN) -Itests/fuzz -Isource -Isource/kit -I$(KIT) -isystem library/aurore/library/include
report_qr_FUZZ    := $(filter-out tests/%,$(report_qr_SRC)) tests/fuzz/report_qr/fuzz.cpp
report_qr_FUZZ_LIBS := -lzstd
zip_read_FUZZ      := $(KIT)/util/zip_read.cpp tests/fuzz/zip_read/fuzz.cpp
zip_read_FUZZ_LIBS := -lz
changelog_FUZZ     := $(KIT)/util/date.cpp $(KIT)/util/changelog.cpp $(KIT)/util/update.cpp tests/fuzz/changelog/fuzz.cpp
changelog_FUZZ_CSRC := $(CALENDAR_C)
config_FUZZ        := $(filter-out tests/%,$(config_SRC)) tests/fuzz/config/fuzz.cpp
config_FUZZ_CSRC   := $(CALENDAR_C)
install_pfs_FUZZ  := $(filter-out tests/%,$(install_SRC)) tests/fuzz/install_pfs/fuzz.cpp
install_cnmt_FUZZ := $(filter-out tests/%,$(install_SRC)) tests/fuzz/install_cnmt/fuzz.cpp
install_nca_FUZZ  := $(filter-out tests/%,$(install_SRC)) tests/fuzz/install_nca/fuzz.cpp
compressed_storage_FUZZ := $(filter-out tests/%,$(compressed_storage_SRC)) tests/fuzz/compressed_storage/fuzz.cpp
install_stream_FUZZ      := $(filter-out tests/%,$(install_stream_SRC)) tests/fuzz/install_stream/fuzz.cpp
install_stream_FUZZ_LIBS := -lzstd
install_package_FUZZ      := $(filter-out tests/%,$(install_txn_SRC)) tests/fuzz/install_package/fuzz.cpp
install_package_FUZZ_LIBS := -lzstd
install_bktr_FUZZ       := $(filter-out tests/%,$(bktr_SRC)) tests/fuzz/install_bktr/fuzz.cpp
install_ivfc_FUZZ       := $(filter-out tests/%,$(ivfc_SRC)) tests/fuzz/install_ivfc/fuzz.cpp
install_recompress_FUZZ := $(filter-out tests/%,$(recompress_SRC)) tests/fuzz/install_recompress/fuzz.cpp
save_archive_FUZZ      := source/saves/archive.cpp tests/fuzz/save_archive/fuzz.cpp
save_archive_FUZZ_LIBS := -lz
titledb_FUZZ           := $(filter-out tests/%,$(titledb_SRC)) tests/fuzz/titledb/fuzz.cpp
FUZZERS := $(sort $(patsubst %_FUZZ,%,$(filter %_FUZZ,$(.VARIABLES))))
FUZZ_PROGRAMS := $(addprefix $(FUZZOUT)/,$(FUZZERS))
FUZZ_RUNS := $(addsuffix .run,$(FUZZ_PROGRAMS))

$(FUZZOUT):
	@mkdir -p $@

# Rebuilt when the Makefile changes too (the flags above).
$(FUZZ_PROGRAMS): $(FUZZOUT)/%: $$($$*_FUZZ) $$($$*_FUZZ_CSRC) tests/fuzz/fuzz.h $(TEST_HEADERS) Makefile | $(FUZZOUT)
	$(if $($*_FUZZ_CSRC),$(FUZZ_CC) -std=gnu11 $(FUZZ_CFLAGS) $($*_FUZZ_FLAGS) -c $($*_FUZZ_CSRC) -o $@.c.o)
	$(if $(filter %.c,$(lastword $($*_FUZZ))),$(FUZZ_CC) -std=gnu11 -I$(KIT)/core,$(FUZZ_CXX) -std=c++23) \
		$(FUZZ_CFLAGS) $($*_FUZZ_FLAGS) $($*_FUZZ) $(if $($*_FUZZ_CSRC),$@.c.o) $($*_FUZZ_LIBS) -o $@

# A run: its output in <name>.log, its final figures on the terminal (tagged
# with its name), and on a failure what it found, with the input libFuzzer
# saved. The paths are absolute: the config target works in a folder of its
# own (paths::data_dir() is relative).
$(FUZZ_RUNS): $(FUZZOUT)/%.run: $(FUZZOUT)/%
	@mkdir -p $<.corpus
	@rm -f $<-crash-* $<-leak-* $<-timeout-* $<-oom-*
	@$(abspath $<) -max_total_time=$(FUZZ_SECONDS) -timeout=10 -rss_limit_mb=2048 -print_final_stats=1 \
		$(if $(wildcard tests/fuzz/$*/dict),-dict=$(abspath tests/fuzz/$*/dict)) -artifact_prefix=$(abspath $<)- $(abspath $<.corpus) $(abspath tests/fuzz/$*/corpus) > $<.log 2>&1; r=$$?; \
		if [ $$r -eq 0 ]; then grep -E '^(Done|stat::(number_of_executed_units|new_units_added|peak_rss_mb))' $<.log; \
		else grep -v 'Uses:' $<.log | tail -n 40; fi | sed 's/^/[$*] /'; \
		if [ $$r -ne 0 ]; then echo "$*: the fuzzer stopped with status $$r: $$(ls $<-* 2>/dev/null | head -n 1)" >&2; exit $$r; fi

fuzz:
	@test -f $(JSON_HPP) || { echo "Aurore is missing (nlohmann/json for the config target): run git submodule update --init"; exit 1; }
	@$(MAKE) --no-print-directory -k $(if $(findstring -j,$(MAKEFLAGS)),,-j$(JOBS)) -Otarget $(FUZZ_RUNS)

check: test
	python3 tools/check_resources.py .
	python3 tools/check_sources.py .
	python3 tools/gen_compat.py . $(BUILD)/compat.json
	REPORT_QR_FIXTURE=$(BUILD)/report_qr.fixture $(BUILD)/host-tests/report_qr >/dev/null
	python3 tests/decode_report/test.py $(BUILD)/report_qr.fixture

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(DESKTOP) out $(TARGET).zip $(TARGET).nro $(TARGET).nacp $(TARGET).elf

nxlink: all
	nxlink $(BUILD)/$(TARGET).nro
