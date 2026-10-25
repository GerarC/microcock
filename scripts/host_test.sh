#!/usr/bin/env bash
set -eu

CXX_BIN="${CXX:-g++}"
OUTPUT_DIR="${OUTPUT_DIR:-build/host_test}"
BINARY="$OUTPUT_DIR/host_test"

mkdir -p "$OUTPUT_DIR"
"$CXX_BIN" -std=c++20 -Wall -Wextra -O1 \
	-I. -Ishared/include -Iuserland/servers/names/include \
	test/host/*.cpp userland/servers/names/src/services.cpp userland/servers/names/src/policy.cpp userland/servers/names/src/paths.cpp userland/servers/names/src/assembly.cpp userland/servers/names/src/forked.cpp test/userland/fuzz/elf_mutator.cpp \
	-o "$BINARY"
"$BINARY"
