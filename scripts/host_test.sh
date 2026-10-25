#!/usr/bin/env bash
set -eu

CXX_BIN="${CXX:-g++}"
OUTPUT_DIR="${OUTPUT_DIR:-build/host_test}"
BINARY="$OUTPUT_DIR/host_test"

mkdir -p "$OUTPUT_DIR"
"$CXX_BIN" -std=c++20 -Wall -Wextra -O1 \
	-I. -Ishared/include -Iuserland/names/include \
	test/host/*.cpp userland/names/src/services.cpp userland/names/src/policy.cpp userland/names/src/paths.cpp userland/names/src/assembly.cpp userland/names/src/forked.cpp test/userland/fuzz/elf_mutator.cpp \
	-o "$BINARY"
"$BINARY"
