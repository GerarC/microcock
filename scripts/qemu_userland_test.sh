#!/usr/bin/env bash
set -u

BOOT_SECONDS="${BOOT_SECONDS:-6}"
KEY_SECONDS="${KEY_SECONDS:-2}"
SERIAL_FILE="${SERIAL_FILE:-build/userland_serial.txt}"
SOCKET_FILE="${SOCKET_FILE:-build/userland_monitor.sock}"
QEMU_BIN="${QEMU:-qemu-system-i386}"
TYPED_KEYS="a b shift-c d"
EXPECTED_SCREEN_TEXT="Hello from cock ring 3!!!|probe: 9/9 checks passed|abCd"
EXPECTED_SERIAL_TEXT="kbd: ready|VGA driver detected"

make clean "$@" > /dev/null
make iso "$@" || exit 2

mkdir -p "$(dirname "$SERIAL_FILE")"
rm -f "$SOCKET_FILE" "$SERIAL_FILE"

"$QEMU_BIN" -cdrom build/cock.iso -display none -no-reboot \
	-serial "file:$SERIAL_FILE" \
	-monitor "unix:$SOCKET_FILE,server,nowait" &
qemu_pid=$!
sleep "$BOOT_SECONDS"

screen=$(python3 - "$SOCKET_FILE" "$TYPED_KEYS" "$KEY_SECONDS" <<'PYTHON'
import re
import socket
import sys
import time

socket_path, keys, key_seconds = sys.argv[1], sys.argv[2].split(), float(sys.argv[3])
SCREEN_BYTES = 4000
CELL_BYTES = 2
COLUMNS = 80
ROWS = 25
VIDEO_ADDRESS = "0xb8000"

monitor = socket.socket(socket.AF_UNIX)
monitor.connect(socket_path)
time.sleep(0.5)
monitor.recv(65536)


def command(text, wait):
    monitor.send((text + "\n").encode())
    time.sleep(wait)
    return monitor.recv(1 << 20).decode(errors="replace")


for key in keys:
    command("sendkey " + key, 0.3)
time.sleep(key_seconds)

dump = command("xp/%dbx %s" % (SCREEN_BYTES, VIDEO_ADDRESS), 1.5)
cells = []
for line in dump.splitlines():
    match = re.match(r"^[0-9a-f]{8,16}:(.*)$", line.strip())
    if match:
        cells += [int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", match.group(1))]
characters = cells[0::CELL_BYTES]
for row in range(ROWS):
    text = "".join(chr(c) if 32 <= c < 127 else " " for c in characters[row * COLUMNS:(row + 1) * COLUMNS])
    print(text.rstrip())
command("quit", 0.2)
PYTHON
)
kill "$qemu_pid" 2> /dev/null
wait "$qemu_pid" 2> /dev/null

echo "---- screen"
echo "$screen"
echo "---- serial"
cat "$SERIAL_FILE"

status=0
IFS='|' read -ra screen_checks <<< "$EXPECTED_SCREEN_TEXT"
for expected in "${screen_checks[@]}"; do
	if ! echo "$screen" | grep -qF "$expected"; then
		echo "[qemu_userland_test] missing on screen: $expected"
		status=1
	fi
done
IFS='|' read -ra serial_checks <<< "$EXPECTED_SERIAL_TEXT"
for expected in "${serial_checks[@]}"; do
	if ! grep -qF "$expected" "$SERIAL_FILE"; then
		echo "[qemu_userland_test] missing on serial: $expected"
		status=1
	fi
done
if grep -q "Exception{" "$SERIAL_FILE"; then
	echo "[qemu_userland_test] a thread was killed by an exception"
	status=1
fi

if [ "$status" -eq 0 ]; then echo "[qemu_userland_test] userland works end to end"; fi
exit "$status"
