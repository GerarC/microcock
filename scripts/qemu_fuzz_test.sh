#!/usr/bin/env bash
set -u

FUZZ_MODE="${FUZZ:-user}"
FUZZ_SEED_VALUE="${SEED:-1}"
FUZZ_ROUNDS_VALUE="${ROUNDS:-2000}"
TIMEOUT_SECONDS="${FUZZ_TIMEOUT:-90}"
POLL_SECONDS=1
SERIAL_FILE="${SERIAL_FILE:-build/fuzz_serial.txt}"
QEMU_BIN="${QEMU:-qemu-system-i386}"
DONE_TEXT="fuzz: done seed $FUZZ_SEED_VALUE"
SANITY_TEXT="fuzz: sanity ok"
PANIC_TEXT="cock panic"
EXIT_PANIC=1
EXIT_MAIN_KILLED=2
EXIT_BUILD=6
EXIT_TRIPLE_FAULT=3
EXIT_HANG=4
EXIT_SANITY=5

make iso TEST_INITRD=1 FUZZ="$FUZZ_MODE" FUZZ_SEED="$FUZZ_SEED_VALUE" FUZZ_ROUNDS="$FUZZ_ROUNDS_VALUE" "$@" || exit "$EXIT_BUILD"

mkdir -p "$(dirname "$SERIAL_FILE")"
rm -f "$SERIAL_FILE"

"$QEMU_BIN" -cdrom build/cock.iso -boot d -display none -no-reboot -serial "file:$SERIAL_FILE" &
qemu_pid=$!

waited=0
while [ "$waited" -lt "$TIMEOUT_SECONDS" ]; do
	grep -qF "$DONE_TEXT" "$SERIAL_FILE" 2> /dev/null && break
	grep -qF "$PANIC_TEXT" "$SERIAL_FILE" 2> /dev/null && break
	kill -0 "$qemu_pid" 2> /dev/null || break
	sleep "$POLL_SECONDS"
	waited=$((waited + POLL_SECONDS))
done

alive=0
kill -0 "$qemu_pid" 2> /dev/null && alive=1
kill "$qemu_pid" 2> /dev/null
wait "$qemu_pid" 2> /dev/null

cat "$SERIAL_FILE"
echo "---- fuzz mode $FUZZ_MODE seed $FUZZ_SEED_VALUE rounds $FUZZ_ROUNDS_VALUE"

main_thread=$(grep -o "fuzz: main thread [0-9]*" "$SERIAL_FILE" | head -n 1 | grep -o "[0-9]*$")
last_checkpoint=$(grep "fuzz: round" "$SERIAL_FILE" | tail -n 1)

if grep -qF "$PANIC_TEXT" "$SERIAL_FILE"; then
	echo "[qemu_fuzz_test] kernel panic; last checkpoint: ${last_checkpoint:-none}"
	exit "$EXIT_PANIC"
fi
if [ -n "$main_thread" ] && grep -q "Thread $main_thread killed by exception" "$SERIAL_FILE"; then
	echo "[qemu_fuzz_test] the fuzzer main thread was killed by an exception; last checkpoint: ${last_checkpoint:-none}"
	exit "$EXIT_MAIN_KILLED"
fi
if ! grep -qF "$DONE_TEXT" "$SERIAL_FILE"; then
	if [ "$alive" -eq 0 ]; then
		echo "[qemu_fuzz_test] QEMU ended without finishing (triple fault); last checkpoint: ${last_checkpoint:-none}"
		exit "$EXIT_TRIPLE_FAULT"
	fi
	echo "[qemu_fuzz_test] the run did not finish in $TIMEOUT_SECONDS seconds (hang); last checkpoint: ${last_checkpoint:-none}"
	exit "$EXIT_HANG"
fi
if ! grep -qF "$SANITY_TEXT" "$SERIAL_FILE"; then
	echo "[qemu_fuzz_test] the kernel did not pass the sanity check after the run"
	exit "$EXIT_SANITY"
fi

echo "[qemu_fuzz_test] survived seed $FUZZ_SEED_VALUE in mode $FUZZ_MODE"
exit 0
