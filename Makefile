.RECIPEPREFIX := >

PROJECT := cifrasync
BUILD_DIR := build
BIN_DIR := bin

SRC_BOOTSTRAP := src/main.c src/cli/commands.c
TEST_MAIN := tests/test_main.c

CC := gcc
CSTD ?= c11

WARN_FLAGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion
COMMON_FLAGS := -std=$(CSTD) $(WARN_FLAGS) -Iinclude
RELEASE_FLAGS := -O2
DEBUG_FLAGS := -O0 -g3 -DDEBUG

LDLIBS ?=
LDFLAGS ?=

BOOTSTRAP_OBJ := $(BUILD_DIR)/main.o
COMMANDS_OBJ := $(BUILD_DIR)/commands.o
PARSER_OBJ := $(BUILD_DIR)/parser.o
LOG_OBJ := $(BUILD_DIR)/log.o
PATH_OBJ := $(BUILD_DIR)/path.o
MEMORY_OBJ := $(BUILD_DIR)/memory.o
CODEC_OBJ := $(BUILD_DIR)/codec.o
HASH_OBJ := $(BUILD_DIR)/hash.o
CHUNKER_OBJ := $(BUILD_DIR)/chunker.o
MANIFEST_OBJ := $(BUILD_DIR)/manifest.o
KDF_OBJ := $(BUILD_DIR)/kdf.o
CIPHER_OBJ := $(BUILD_DIR)/cipher.o
KEY_CACHE_OBJ := $(BUILD_DIR)/key_cache.o
CONFIG_OBJ := $(BUILD_DIR)/config.o
THREAD_POOL_OBJ := $(BUILD_DIR)/thread_pool.o
TIME_UTILS_OBJ := $(BUILD_DIR)/time_utils.o
IO_UTILS_OBJ := $(BUILD_DIR)/io_utils.o
REPO_OBJ := $(BUILD_DIR)/repo.o
CHUNK_STORE_OBJ := $(BUILD_DIR)/chunk_store.o
INDEX_STORE_OBJ := $(BUILD_DIR)/index_store.o
SNAPSHOT_STORE_OBJ := $(BUILD_DIR)/snapshot_store.o
ENGINE_OBJ := $(BUILD_DIR)/engine.o
PLANNER_OBJ := $(BUILD_DIR)/planner.o
JOURNAL_OBJ := $(BUILD_DIR)/journal.o
TEST_OBJ := $(BUILD_DIR)/test_main.o

ifeq ($(OS),Windows_NT)
EXE_EXT := .exe
RM_DIR = powershell -NoProfile -Command "if (Test-Path '$(1)') { Remove-Item -Recurse -Force '$(1)' }"
MKDIR = if not exist "$(1)" mkdir "$(1)"
RUN_APP = .\\$(BIN_DIR)\\$(PROJECT)$(EXE_EXT)
RUN_TESTS = .\\$(BIN_DIR)\\$(PROJECT)_tests$(EXE_EXT)
else
EXE_EXT :=
RM_DIR = rm -rf $(1)
MKDIR = mkdir -p $(1)
RUN_APP = ./$(BIN_DIR)/$(PROJECT)$(EXE_EXT)
RUN_TESTS = ./$(BIN_DIR)/$(PROJECT)_tests$(EXE_EXT)
endif

APP := $(BIN_DIR)/$(PROJECT)$(EXE_EXT)
TEST_BIN := $(BIN_DIR)/$(PROJECT)_tests$(EXE_EXT)

.PHONY: all release debug asan test run clean help

all: release

release: CFLAGS := $(COMMON_FLAGS) $(RELEASE_FLAGS)
release: $(APP)

debug: CFLAGS := $(COMMON_FLAGS) $(DEBUG_FLAGS)
debug: $(APP)

asan: CFLAGS := $(COMMON_FLAGS) $(DEBUG_FLAGS) -fsanitize=address,undefined
asan: LDFLAGS += -fsanitize=address,undefined
asan: $(APP)

test: CFLAGS := $(COMMON_FLAGS) $(DEBUG_FLAGS)
test: $(TEST_BIN)
>$(RUN_TESTS)

run: release
>$(RUN_APP)

$(APP): $(BOOTSTRAP_OBJ) $(COMMANDS_OBJ) $(PARSER_OBJ) $(LOG_OBJ) $(PATH_OBJ) $(MEMORY_OBJ) $(CODEC_OBJ) $(HASH_OBJ) $(CHUNKER_OBJ) $(MANIFEST_OBJ) $(KDF_OBJ) $(CIPHER_OBJ) $(KEY_CACHE_OBJ) $(CONFIG_OBJ) $(THREAD_POOL_OBJ) $(TIME_UTILS_OBJ) $(IO_UTILS_OBJ) $(REPO_OBJ) $(CHUNK_STORE_OBJ) $(INDEX_STORE_OBJ) $(SNAPSHOT_STORE_OBJ) $(ENGINE_OBJ) $(PLANNER_OBJ) $(JOURNAL_OBJ)
>@$(call MKDIR,$(BIN_DIR))
>$(CC) $(BOOTSTRAP_OBJ) $(COMMANDS_OBJ) $(PARSER_OBJ) $(LOG_OBJ) $(PATH_OBJ) $(MEMORY_OBJ) $(CODEC_OBJ) $(HASH_OBJ) $(CHUNKER_OBJ) $(MANIFEST_OBJ) $(KDF_OBJ) $(CIPHER_OBJ) $(KEY_CACHE_OBJ) $(CONFIG_OBJ) $(THREAD_POOL_OBJ) $(TIME_UTILS_OBJ) $(IO_UTILS_OBJ) $(REPO_OBJ) $(CHUNK_STORE_OBJ) $(INDEX_STORE_OBJ) $(SNAPSHOT_STORE_OBJ) $(ENGINE_OBJ) $(PLANNER_OBJ) $(JOURNAL_OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(TEST_BIN): $(TEST_OBJ)
>@$(call MKDIR,$(BIN_DIR))
>$(CC) $(TEST_OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/main.o: $(SRC_BOOTSTRAP)
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/commands.o: src/cli/commands.c include/cli/commands.h include/cli/parser.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/parser.o: src/cli/parser.c include/cli/parser.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/log.o: src/common/log.c include/common/log.h include/common/constants.h include/util/time_utils.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/path.o: src/common/path.c include/common/path.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/memory.o: src/common/memory.c include/common/memory.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/codec.o: src/compress/codec.c include/compress/codec.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/hash.o: src/delta/hash.c include/delta/hash.h include/crypto/kdf.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/chunker.o: src/delta/chunker.c include/delta/chunker.h include/delta/hash.h include/common/memory.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/manifest.o: src/delta/manifest.c include/delta/manifest.h include/delta/chunker.h include/common/memory.h include/common/path.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kdf.o: src/crypto/kdf.c include/crypto/kdf.h include/common/memory.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cipher.o: src/crypto/cipher.c include/crypto/cipher.h include/crypto/kdf.h include/common/memory.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/key_cache.o: src/crypto/key_cache.c include/crypto/key_cache.h include/crypto/kdf.h include/common/memory.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/config.o: src/util/config.c include/util/config.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/thread_pool.o: src/util/thread_pool.c include/util/thread_pool.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/time_utils.o: src/util/time_utils.c include/util/time_utils.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/io_utils.o: src/util/io_utils.c include/util/io_utils.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/repo.o: src/storage/repo.c include/storage/repo.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/chunk_store.o: src/storage/chunk_store.c include/storage/chunk_store.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/index_store.o: src/storage/index_store.c include/storage/index_store.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/snapshot_store.o: src/storage/snapshot_store.c include/storage/snapshot_store.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/engine.o: src/core/engine.c include/core/engine.h include/common/log.h include/core/journal.h include/core/planner.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/planner.o: src/core/planner.c include/core/planner.h include/common/path.h include/common/log.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/journal.o: src/core/journal.c include/core/journal.h include/common/path.h include/common/log.h
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_main.o: $(TEST_MAIN)
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

clean:
>@$(call RM_DIR,$(BUILD_DIR))
>@$(call RM_DIR,$(BIN_DIR))

help:
>@echo CifraSync bootstrap Makefile
>@echo.
>@echo Targets:
>@echo   make release  - Build optimized bootstrap binary
>@echo   make debug    - Build debug bootstrap binary
>@echo   make asan     - Build with ASan and UBSan
>@echo   make test     - Build and run bootstrap tests
>@echo   make run      - Build release and run app
>@echo   make clean    - Remove build and bin output
>@echo   make help     - Show this help message
>@echo.
>@echo Notes:
>@echo   On Windows use mingw32-make if make is unavailable.
>@echo   This Makefile currently compiles src/main.c and tests/test_main.c.
