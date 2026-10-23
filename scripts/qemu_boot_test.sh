#!/usr/bin/env bash
set -u

TIMEOUT_SECONDS="${TIMEOUT_SECONDS:-10}"
TIMED_OUT_STATUS=124
QEMU_BIN="${QEMU:-qemu-system-i386}"
OUTPUT_FILE="${OUTPUT_FILE:-build/boot_serial.txt}"
KERNEL_IMAGE=build/cock.bin
EXPECTED_LINES="Welcome to|Boot memory released|Idle thread running"
FORBIDDEN_LINES="Setting Test mode|Loading module|Kernel panic|Exception\{|Cannot "

make "$KERNEL_IMAGE" "$@" || exit 6

mkdir -p "$(dirname "$OUTPUT_FILE")"
timeout "$TIMEOUT_SECONDS" "$QEMU_BIN" -kernel "$KERNEL_IMAGE" \
	-display none -serial stdio -no-reboot > "$OUTPUT_FILE" 2>&1
status=$?

cat "$OUTPUT_FILE"

if [ "$status" -ne "$TIMED_OUT_STATUS" ]; then
	echo "[qemu_boot_test] QEMU ended with status $status before the time limit (reset or exit)"
	exit 1
fi

IFS='|' read -ra expected <<< "$EXPECTED_LINES"
for line in "${expected[@]}"; do
	if ! grep -q -- "$line" "$OUTPUT_FILE"; then
		echo "[qemu_boot_test] missing on serial: $line"
		exit 2
	fi
done

if grep -q -E "$FORBIDDEN_LINES" "$OUTPUT_FILE"; then
	echo "[qemu_boot_test] unexpected line on serial:"
	grep -E "$FORBIDDEN_LINES" "$OUTPUT_FILE"
	exit 3
fi

echo "[qemu_boot_test] the kernel boots with no user program and reaches the idle thread"
