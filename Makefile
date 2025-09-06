# ---------------------------------------
# Project Makefile
# ---------------------------------------
# Usage: `make help` to list targets.
# Put a short description after `##` on each target to show up in help.

# Compiler to use
CC     = clang
# Debugger to use
DBG    = lldb
# Compiler flags for debugging
CFLAGS = -std=c23 -Wall -Wextra -O0 -g
# Source code directory
SRC    = ./src
# Output directory for compiled binaries
BIN    = ./bin

# Colors for pretty help
BLUE := \033[1;34m
NC   := \033[0m

# Make `make` default to help
.DEFAULT_GOAL := help

.PHONY: clean run test debug debug-test \
		build-main build-test create-dir check help

## help: Show this help
help:
	@printf "%b\n" "$(BLUE)Usage: make <target>$(NC)"
	@awk '/^## / {                                   \
		line = substr($$0, 4);                       \
		i = index(line, ":");                        \
		if (i) {                                     \
			name = substr(line, 1, i-1);             \
			desc = substr(line, i+1);                \
			gsub(/^[ \t]+/, "", desc);               \
			printf "  \033[36m%-20s\033[0m %s\n",    \
			       name, desc;                       \
		}                                            \
	}' $(MAKEFILE_LIST)

## check: Verifies if clang and lldb are installed
check:
	@which $(CC) > /dev/null \
	    && echo "✅ $(CC) is installed" \
	    || echo "⚠️ $(CC) not found, please install clang"
	@which $(DBG) > /dev/null \
	    && echo "✅ $(DBG) is installed" \
	    || echo "⚠️ $(DBG) not found, please install lldb"

## create-dir: Creates the bin directory if it doesn't exist
create-dir:
	@if [ ! -d $(BIN) ]; then mkdir $(BIN); fi

## build-main: Compiles the main program
build-main: create-dir
	$(CC) $(CFLAGS) -o $(BIN)/main $(SRC)/main.c

## build-test: Compiles the test program
build-test: create-dir
	$(CC) $(CFLAGS) -o $(BIN)/test $(SRC)/test.c

## run: Executes the compiled main program
run: build-main
	$(BIN)/main

## test: Executes the compiled test program
test: build-test
	$(BIN)/test

## debug: Runs the main program in the debugger (lldb)
debug: build-main
	$(DBG) $(BIN)/main

## debug-test: Runs the test program in the debugger (lldb)
debug-test: build-test
	$(DBG) $(BIN)/test

## clean: Removes the 'bin' directory and all compiled binaries
clean:
	rm -rf $(BIN)
