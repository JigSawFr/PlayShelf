# Thin shim over CMake — the real build is in CMakeLists.txt.
#
#   make              -> ./playshelf.nro            (needs devkitPro, DEVKITPRO set)
#   make dist         -> ./playshelf.zip            (unzip onto the SD card root)
#   make desktop      -> ./build-desktop/playshelf  (UI with a simulated backend; needs GLFW)
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
DESKTOP := build-desktop
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
JSON_HPP := extern/borealis/library/include/borealis/extern/nlohmann/json.hpp

.PHONY: all clean dist nxlink desktop test fuzz check FORCE

RENDERER := $(if $(GL),-DUSE_DEKO3D=OFF,-DUSE_DEKO3D=ON)

all:
	@cmake -B $(BUILD) -S . -DPLATFORM_SWITCH=ON $(RENDERER) $(CMAKE_ARGS)
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

desktop:
	@cmake -B $(DESKTOP) -S . -DPLATFORM_DESKTOP=ON -DCMAKE_BUILD_TYPE=Release $(CMAKE_ARGS)
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
calendar_SRC   := source/kit/core/calendar.c tests/calendar/test.c
calendar_FLAGS := -std=gnu11 -Isource/kit/core
startup_SRC   := source/kit/util/startup.c tests/startup/test.c
startup_FLAGS := -std=gnu11 -Isource/kit/util
crash_dump_SRC   := source/kit/util/crash_dump.c tests/crash_dump/test.c
crash_dump_FLAGS := -std=c11 -Isource/kit/util
write_guard_SRC   := source/kit/core/write_guard.c tests/write_guard/test.c
write_guard_FLAGS := -std=gnu11 -Isource/kit/core

# C++: the UI-free helpers and flow logic (source/kit/util, source/kit/action,
# source/util), by default with TEST_CXXFLAGS (a <name>_FLAGS only where a
# suite differs). borealis' headers (nlohmann/json) as system headers: their
# warnings are not PlayShelf's (clang 22 deprecates json.hpp's literal
# operators, under -Werror). util/date stands on core/calendar.c: a suite
# that links it builds that C file too (its <name>_CSRC, without having to
# say so).
TEST_CXXFLAGS   := -std=c++17 -Isource -Isource/kit -isystem extern/borealis/library/include
CALENDAR_C      := source/kit/core/calendar.c
CALENDAR_CFLAGS := -std=gnu11 -Isource/kit/core
K := source/kit
# Aurore's Cadran (extern/borealis): the kit's UI helpers, tested here.
C := extern/borealis/library/lib/cadran
# A secret sealed for this console: AES and HMAC, and the console's key
# (the desktop build's random one: sim/sim_device_key.cpp).
SECRET_BOX := $(K)/util/secret_box.cpp $(K)/util/aes.cpp $(K)/sim/sim_device_key.cpp
secret_box_SRC := $(SECRET_BOX) $(K)/util/sha256.cpp $(K)/util/paths.cpp tests/secret_box/test.cpp
# The install formats (source/install/: crypto, PFS0/HFS0, XCI, CNMT, NCA),
# pure C++ with the kit's SHA-256.
INSTALL_FORMATS := $(K)/util/aes.cpp source/install/rsa.cpp source/install/pfs.cpp source/install/cnmt.cpp \
                   source/install/nca.cpp $(K)/util/sha256.cpp
install_SRC := $(INSTALL_FORMATS) tests/install/test.cpp
# A content (.nca or .ncz) as it arrives, made the bytes of its NCA; zstd
# from the system (libzstd-dev), compression too, for the NCZ it builds.
install_stream_SRC  := $(INSTALL_FORMATS) source/install/content_stream.cpp tests/install_stream/test.cpp
install_stream_LIBS := -lzstd
# A package (NSP, NSZ, XCI) installed as one transaction, on a console in memory.
install_txn_SRC  := $(INSTALL_FORMATS) source/install/content_stream.cpp source/install/package.cpp \
                    source/install/installer.cpp tests/install_txn/test.cpp
install_txn_LIBS := -lzstd
# The library client's formats (source/library/: catalogues, addresses,
# discovery, endpoints, certificate pins), with the kit's SHA-256.
LIBRARY_FORMATS := source/library/catalog.cpp source/library/server.cpp $(K)/util/sha256.cpp
library_SRC := $(LIBRARY_FORMATS) tests/library/test.cpp
# A server's catalogue fetched (a scripted network) and the servers' file.
library_client_SRC := $(filter-out $(K)/util/sha256.cpp,$(LIBRARY_FORMATS)) source/library/client.cpp source/library/servers.cpp \
                      $(SECRET_BOX) $(K)/util/sha256.cpp $(K)/util/paths.cpp tests/library_client/test.cpp
# The desktop build's console (sim/sim_install.cpp) behind install::Console.
install_sim_SRC  := $(filter-out tests/%,$(install_txn_SRC)) source/install/console_core.cpp source/sim/sim_install.cpp \
                    $(K)/util/paths.cpp $(K)/sim/sim_env.cpp tests/install_sim/test.cpp
install_sim_CSRC := $(K)/sim/sim_sysinfo.c
install_sim_CFLAGS := -std=gnu11 -Isource/kit/core
install_sim_LIBS := -lzstd
date_SRC    := $(K)/util/date.cpp tests/date/test.cpp
# The header-only helpers: utf8, emoji, text, json_util.
text_util_SRC   := tests/text_util/test.cpp
patches_SRC   := $(K)/util/paths.cpp $(K)/util/patches.cpp tests/patches/test.cpp
changelog_SRC   := $(K)/util/changelog.cpp tests/changelog/test.cpp
support_SRC   := $(K)/util/date.cpp $(K)/util/support.cpp tests/support/test.cpp
write_queue_logic_SRC   := $(K)/action/write_queue_logic.cpp tests/write_queue_logic/test.cpp
write_queue_logic_LIBS  := -pthread
worker_threads_SRC   := $(K)/util/worker_threads.cpp tests/worker_threads/test.cpp
worker_threads_LIBS  := -pthread
guarded_SRC          := tests/guarded/test.cpp
data_cleanup_SRC   := $(K)/util/data_cleanup.cpp tests/data_cleanup/test.cpp
error_hint_logic_SRC   := $(K)/action/error_hint_logic.cpp tests/error_hint_logic/test.cpp
http_logic_SRC   := $(K)/util/http_logic.cpp tests/http_logic/test.cpp
update_SRC   := $(K)/util/date.cpp $(K)/util/update.cpp tests/update/test.cpp
config_SRC   := $(K)/util/paths.cpp $(K)/util/date.cpp source/util/config.cpp tests/config/test.cpp
launcher_SRC   := $(K)/util/paths.cpp $(K)/util/launcher.cpp tests/launcher/test.cpp
markdown_SRC   := $(C)/util/safe_text.cpp $(C)/util/markdown.cpp tests/markdown/test.cpp
rollback_SRC   := $(K)/util/paths.cpp $(K)/util/rollback.cpp tests/rollback/test.cpp
log_upload_SRC   := $(K)/util/paths.cpp $(K)/util/log_files.cpp $(K)/util/log_upload.cpp tests/log_upload/test.cpp
dev_builds_SRC   := $(K)/util/paths.cpp $(K)/util/sha256.cpp $(K)/util/date.cpp $(K)/util/update.cpp $(C)/util/safe_text.cpp $(K)/util/dev_builds.cpp $(K)/util/zip_read.cpp $(K)/util/github_auth.cpp $(SECRET_BOX) tests/dev_builds/test.cpp
dev_builds_LIBS  := -lz
# The download and its resume: without PLAYSHELF_SIM_DEV_BUILDS the desktop
# build takes the console's path, here against a fake http::download and
# http::redirect in the test (-D__SWITCH__ would reach date.cpp's switch.h).
dev_builds_net_SRC   := $(filter-out tests/%,$(dev_builds_SRC)) $(K)/util/http_logic.cpp $(K)/util/zip_write.cpp $(K)/sim/sim_net.cpp $(K)/sim/sim_env.cpp $(K)/util/dev_builds_net.cpp tests/dev_builds_net/test.cpp
dev_builds_net_LIBS  := -lz
perf_SRC   := $(K)/util/perf.cpp tests/perf/test.cpp
# fmt header-only: the report's lines are formatted with it.
diagnostics_logic_SRC   := $(K)/util/paths.cpp $(K)/util/patches.cpp $(K)/util/diagnostics_logic.cpp tests/diagnostics_logic/test.cpp
diagnostics_logic_FLAGS := $(TEST_CXXFLAGS) -isystem extern/borealis/library/lib/extern/fmt/include -DFMT_HEADER_ONLY
# The text helpers that only translate, over Cadran's ui/tr.hpp stand-in for
# borealis' getStr and fmt header-only (the catalogs read by the test).
format_text_SRC   := $(C)/ui/format_text.cpp tests/format_text/test.cpp
format_text_FLAGS := $(TEST_CXXFLAGS) -isystem extern/borealis/library/lib/extern/fmt/include -DFMT_HEADER_ONLY -DBRLS_CADRAN_HOST_I18N
cadran_text_SRC   := $(C)/ui/cadran_text.cpp tests/cadran_text/test.cpp
contrast_SRC      := tests/contrast/test.cpp
cadran_text_FLAGS := $(format_text_FLAGS)
# borealis' own string tables (core/i18n.cpp), read from other threads while
# the language changes. A copy of the files at the submodule's commit, first
# in the include path, made again when that commit changes (the stamp holds
# it, rewritten only then).
I18N_COPY    := $(TESTOUT)/borealis_i18n
I18N_STAMP   := $(TESTOUT)/borealis_i18n.commit
I18N_COMMIT  := $(shell git -C extern/borealis rev-parse HEAD 2>/dev/null)
$(I18N_STAMP): FORCE | $(TESTOUT)
	@echo '$(I18N_COMMIT)' | cmp -s - $@ || echo '$(I18N_COMMIT)' > $@
$(I18N_COPY)/i18n.cpp: $(I18N_STAMP) | $(TESTOUT)
	@rm -rf $(I18N_COPY) && mkdir -p $(I18N_COPY)/library/lib/core $(I18N_COPY)/library/include/borealis/core
	@git -C extern/borealis show HEAD:library/lib/core/i18n.cpp > $(I18N_COPY)/library/lib/core/i18n.cpp
	@git -C extern/borealis show HEAD:library/include/borealis/core/i18n.hpp > $(I18N_COPY)/library/include/borealis/core/i18n.hpp
	@git -C extern/borealis show HEAD:library/include/borealis/core/platform.hpp > $(I18N_COPY)/library/include/borealis/core/platform.hpp
	@# The suffix without a space, as newer clang wants it (-Wdeprecated-literal-operator),
	@# in this copy only: borealis itself is built as it is.
	@sed 's/operator"" _i18n/operator""_i18n/' $(I18N_COPY)/library/lib/core/i18n.cpp > $@
i18n_threads_SRC   := $(I18N_COPY)/i18n.cpp tests/i18n_threads/test.cpp
i18n_threads_FLAGS := -std=c++17 -isystem $(I18N_COPY)/library/include -isystem extern/borealis/library/include \
                      -isystem extern/borealis/library/lib/extern/fmt/include -DFMT_HEADER_ONLY \
                      $(addprefix -isystem extern/borealis/library/,include/borealis/extern include/borealis/extern/nanovg \
                        include/borealis/extern/tinyxml2 lib/extern/tweeny/include lib/extern/yoga/src) \
                      -DBRLS_RESOURCES='"./"'
i18n_threads_LIBS  := -pthread
session_log_SRC   := $(K)/util/paths.cpp $(K)/util/log_files.cpp $(K)/util/session_log.cpp $(K)/util/crash_report.cpp tests/session_log/test.cpp
# A C source a C++ suite links (clang++ refuses to build a .c as C++).
session_log_CSRC  := $(K)/util/crash_dump.c
session_log_CFLAGS := -std=c11 -Isource/kit/util
frame_stats_SRC   := $(K)/util/paths.cpp $(K)/util/frame_stats.cpp tests/frame_stats/test.cpp
log_files_SRC   := $(K)/util/paths.cpp $(K)/util/log_files.cpp tests/log_files/test.cpp
migrations_SRC   := $(K)/util/paths.cpp $(K)/util/date.cpp $(K)/util/update.cpp $(K)/util/migrations.cpp tests/migrations/test.cpp

TESTS := $(sort $(patsubst %_SRC,%,$(filter %_SRC,$(.VARIABLES))))
TEST_PROGRAMS := $(addprefix $(TESTOUT)/,$(TESTS))
TEST_OKS := $(addsuffix .ok,$(TEST_PROGRAMS))
# Every header: a suite is rebuilt when one changes (coarse, but a suite
# compiles in seconds and CI always starts cold).
TEST_HEADERS := $(wildcard tests/*.h tests/*.hpp tests/*/*.h tests/*/*.hpp source/*.hpp source/*/*.h source/*/*.hpp source/kit/*/*.h source/kit/*/*.hpp)

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
test_csrc   = $(or $($1_CSRC),$(if $(filter source/kit/util/date.cpp,$($1_SRC)),$(CALENDAR_C)))
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
	@test -f $(JSON_HPP) || { echo "borealis is missing (nlohmann/json for the tests): run git submodule update --init"; exit 1; }
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
FUZZ_CFLAGS  := -Wall -Wextra -Wshadow -Werror -D_GLIBCXX_ASSERTIONS $(FUZZ_SAN) -Itests/fuzz -Isource -Isource/kit -isystem extern/borealis/library/include
zip_read_FUZZ      := source/kit/util/zip_read.cpp tests/fuzz/zip_read/fuzz.cpp
zip_read_FUZZ_LIBS := -lz
markdown_FUZZ      := $(filter-out tests/%,$(markdown_SRC)) tests/fuzz/markdown/fuzz.cpp
changelog_FUZZ     := $(filter-out tests/%,$(changelog_SRC)) tests/fuzz/changelog/fuzz.cpp
config_FUZZ        := $(filter-out tests/%,$(config_SRC)) tests/fuzz/config/fuzz.cpp
config_FUZZ_CSRC   := $(CALENDAR_C)
install_pfs_FUZZ  := $(filter-out tests/%,$(install_SRC)) tests/fuzz/install_pfs/fuzz.cpp
install_cnmt_FUZZ := $(filter-out tests/%,$(install_SRC)) tests/fuzz/install_cnmt/fuzz.cpp
install_nca_FUZZ  := $(filter-out tests/%,$(install_SRC)) tests/fuzz/install_nca/fuzz.cpp
install_stream_FUZZ      := $(filter-out tests/%,$(install_stream_SRC)) tests/fuzz/install_stream/fuzz.cpp
install_stream_FUZZ_LIBS := -lzstd
install_package_FUZZ      := $(filter-out tests/%,$(install_txn_SRC)) tests/fuzz/install_package/fuzz.cpp
install_package_FUZZ_LIBS := -lzstd
library_catalog_FUZZ   := $(LIBRARY_FORMATS) tests/fuzz/library_catalog/fuzz.cpp
library_discovery_FUZZ := $(LIBRARY_FORMATS) tests/fuzz/library_discovery/fuzz.cpp
FUZZERS := $(sort $(patsubst %_FUZZ,%,$(filter %_FUZZ,$(.VARIABLES))))
FUZZ_PROGRAMS := $(addprefix $(FUZZOUT)/,$(FUZZERS))
FUZZ_RUNS := $(addsuffix .run,$(FUZZ_PROGRAMS))

$(FUZZOUT):
	@mkdir -p $@

# Rebuilt when the Makefile changes too (the flags above).
$(FUZZ_PROGRAMS): $(FUZZOUT)/%: $$($$*_FUZZ) $$($$*_FUZZ_CSRC) tests/fuzz/fuzz.h $(TEST_HEADERS) Makefile | $(FUZZOUT)
	$(if $($*_FUZZ_CSRC),$(FUZZ_CC) -std=gnu11 $(FUZZ_CFLAGS) $($*_FUZZ_FLAGS) -c $($*_FUZZ_CSRC) -o $@.c.o)
	$(if $(filter %.c,$(lastword $($*_FUZZ))),$(FUZZ_CC) -std=gnu11 -Isource/kit/core,$(FUZZ_CXX) -std=c++17) \
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
	@test -f $(JSON_HPP) || { echo "borealis is missing (nlohmann/json for the config target): run git submodule update --init"; exit 1; }
	@$(MAKE) --no-print-directory -k $(if $(findstring -j,$(MAKEFLAGS)),,-j$(JOBS)) -Otarget $(FUZZ_RUNS)

check: test
	python3 tools/check_resources.py .
	python3 tools/check_sources.py .
	python3 tools/gen_compat.py . $(BUILD)/compat.json

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(DESKTOP) out $(TARGET).zip $(TARGET).nro $(TARGET).nacp $(TARGET).elf

nxlink: all
	nxlink $(BUILD)/$(TARGET).nro
