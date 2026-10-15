#!/usr/bin/env bash
set -eu

CXX_BIN="${CXX:-g++}"
OUTPUT_DIR="${OUTPUT_DIR:-build/host_test}"
BINARY="$OUTPUT_DIR/host_test"

mkdir -p "$OUTPUT_DIR"
"$CXX_BIN" -std=c++20 -Wall -Wextra -O1 \
	-I. -Ishared/include \
	test/host/*.cpp -o "$BINARY"
"$BINARY"
