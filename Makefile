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

$(APP): $(BOOTSTRAP_OBJ) $(COMMANDS_OBJ)
>@$(call MKDIR,$(BIN_DIR))
>$(CC) $(BOOTSTRAP_OBJ) $(COMMANDS_OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(TEST_BIN): $(TEST_OBJ)
>@$(call MKDIR,$(BIN_DIR))
>$(CC) $(TEST_OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/main.o: $(SRC_BOOTSTRAP)
>@$(call MKDIR,$(BUILD_DIR))
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/commands.o: src/cli/commands.c include/cli/commands.h include/cli/parser.h
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
