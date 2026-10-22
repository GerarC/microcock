#!/usr/bin/env bash
set -u

TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-60}"
SUCCESS_STATUS=33
FAILURE_STATUS=35
QEMU_BIN="${QEMU:-qemu-system-i386}"
OUTPUT_FILE="${OUTPUT_FILE:-build/test_output.txt}"

make iso TEST_BUILD=1 "$@" || exit 2

mkdir -p "$(dirname "$OUTPUT_FILE")"
timeout "$TIMEOUT_SECONDS" "$QEMU_BIN" -cdrom build/cock.iso \
	-display none -serial stdio -no-reboot \
	-device isa-debug-exit,iobase=0xf4,iosize=0x04 > "$OUTPUT_FILE" 2>&1
status=$?

cat "$OUTPUT_FILE"

case "$status" in
	"$SUCCESS_STATUS") echo "[qemu_test] all kernel tests passed"; exit 0 ;;
	"$FAILURE_STATUS") echo "[qemu_test] kernel tests failed"; exit 1 ;;
	124) echo "[qemu_test] timed out after ${TIMEOUT_SECONDS}s"; exit 3 ;;
	*) echo "[qemu_test] unexpected QEMU status $status"; exit 4 ;;
esac
