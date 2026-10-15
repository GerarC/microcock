#!/usr/bin/env bash
set -u

BOOT_SECONDS="${BOOT_SECONDS:-6}"
KEY_SECONDS="${KEY_SECONDS:-2}"
CLIENTS_TIMEOUT="${CLIENTS_TIMEOUT:-60}"
POLL_SECONDS=1
CLIENTS_DONE_TEXT=("fatclient: ok" "extclient: ok" "cfile: ok" "muteclient: ok")
SERIAL_FILE="${SERIAL_FILE:-build/userland_serial.txt}"
SOCKET_FILE="${SOCKET_FILE:-build/userland_monitor.sock}"
QEMU_BIN="${QEMU:-qemu-system-i386}"
TYPED_KEYS="a b shift-c d"
FILE_CLIENT_RUNS=10
DISK_SCHEME="${DISK_SCHEME:-mbr}"
DISK_FILE="${DISK_FILE:-build/test_disk_$DISK_SCHEME.img}"
EXPECTED_SCREEN_TEXT="Hello from cock ring 3!!!|probe: 34/34 checks passed|abCd"
EXPECTED_SERIAL_TEXT="abort()|probe: printf -42 4294967295 FF hi Z 0xBEEF|probe: initrd says Hello from the cock initrd!!!|init: initrd is boot module|Boot memory released|init: manifest has 18 entries|init: configured vga_driver|init: started kbd|init: started ata|init: started rtc|rtc: ready|init: started fat|init: started vfs|vfs: 4 mounts|init: started probe|kbd: ready|VGA driver detected|reaped"

. "$(dirname "$0")/test_disk_layout.sh"
case "$DISK_SCHEME" in
	gpt) PARTITION_KIND=$KIND_GPT_PARTITION ;;
	*) PARTITION_KIND=$KIND_MBR_PARTITION ;;
esac
EXPECTED_SERIAL_TEXT="$EXPECTED_SERIAL_TEXT|ata: $DISK_SECTORS sectors, $DEVICE_COUNT devices|blkclient: device 0: kind $KIND_DISK first $DISK_FIRST_SECTOR sectors $DISK_SECTORS|blkclient: device 1: kind $PARTITION_KIND first $FIRST_START sectors $FIRST_SECTORS|blkclient: device 2: kind $PARTITION_KIND first $SECOND_START sectors $SECOND_SECTORS|blkclient: sector says $MARKER_TEXT|blkclient: device 3: kind $PARTITION_KIND first $THIRD_START sectors $THIRD_SECTORS|blkclient: device 4: kind $PARTITION_KIND first $FOURTH_START sectors $FOURTH_SECTORS|blkclient: ok|fat: device $FAT_DEVICE,|fatclient: hello says $FAT_HELLO_TEXT|fatclient: ok|ext: device $EXT_DEVICE,|ext: device $EXT4_DEVICE: unsupported feature extents|extclient: small says $EXT_SMALL_TEXT|extclient: ok|muteclient: dead server did not hang the vfs|muteclient: ok|cfile: ext ok|cfile: fat ok|cfile: directories ok|cfile: ok"

make clean "$@" > /dev/null
make iso "$@" || exit 2

mkdir -p "$(dirname "$SERIAL_FILE")"
rm -f "$SOCKET_FILE" "$SERIAL_FILE"
bash "$(dirname "$0")/make_test_disk.sh" "$DISK_SCHEME" "$DISK_FILE" || exit 2

"$QEMU_BIN" -cdrom build/cock.iso -boot d -display none -no-reboot \
	-drive "file=$DISK_FILE,format=raw,if=ide,index=0,media=disk" \
	-serial "file:$SERIAL_FILE" \
	-monitor "unix:$SOCKET_FILE,server,nowait" &
qemu_pid=$!
sleep "$BOOT_SECONDS"
waited=0
while [ "$waited" -lt "$CLIENTS_TIMEOUT" ]; do
	pending=0
	for marker in "${CLIENTS_DONE_TEXT[@]}"; do
		grep -qF "$marker" "$SERIAL_FILE" 2> /dev/null || pending=1
	done
	[ "$pending" -eq 0 ] && break
	sleep "$POLL_SECONDS"
	waited=$((waited + POLL_SECONDS))
done

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
PROMPT = b"(qemu)"
READ_TIMEOUT = 5.0
CHUNK_BYTES = 1 << 16

monitor = socket.socket(socket.AF_UNIX)
monitor.connect(socket_path)
time.sleep(0.5)
monitor.recv(65536)


def command(text, wait):
    monitor.send((text + "\n").encode())
    time.sleep(wait)
    monitor.settimeout(READ_TIMEOUT)
    data = b""
    try:
        while not data.rstrip().endswith(PROMPT):
            chunk = monitor.recv(CHUNK_BYTES)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    return data.decode(errors="replace")


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
served=$(grep -c "fsclient: ok" "$SERIAL_FILE")
if [ "$served" -ne "$FILE_CLIENT_RUNS" ]; then
	echo "[qemu_userland_test] file clients served: $served of $FILE_CLIENT_RUNS"
	status=1
fi
PART_DIR=$(mktemp -d)
FAT_PART="$PART_DIR/fat.img"
EXT_PART="$PART_DIR/ext.img"
dd if="$DISK_FILE" of="$FAT_PART" bs=$SECTOR_BYTES skip=$SECOND_START count=$SECOND_SECTORS status=none
dd if="$DISK_FILE" of="$EXT_PART" bs=$SECTOR_BYTES skip=$THIRD_START count=$THIRD_SECTORS status=none
if ! fsck.fat -n "$FAT_PART" > "$PART_DIR/fat.log" 2>&1; then
	cat "$PART_DIR/fat.log"
	echo "[qemu_userland_test] fsck.fat found problems after the writes"
	status=1
fi
if ! e2fsck -fn "$EXT_PART" > "$PART_DIR/ext.log" 2>&1; then
	cat "$PART_DIR/ext.log"
	echo "[qemu_userland_test] e2fsck found problems after the writes"
	status=1
fi
fat_keep=$(mtype -i "$FAT_PART" "::/$FAT_SCRATCH/$WRITE_KEEP_NAME" 2>&1)
if [ "$fat_keep" != "$WRITE_KEEP_TEXT" ]; then
	echo "[qemu_userland_test] FAT file written by the guest reads back as: $fat_keep"
	status=1
fi
if mtype -i "$FAT_PART" "::/$FAT_SCRATCH/$WRITE_BIG_NAME" > /dev/null 2>&1; then
	echo "[qemu_userland_test] FAT file deleted by the guest is still there"
	status=1
fi
if mtype -i "$FAT_PART" "::/$FAT_SCRATCH/$WRITE_GONE_NAME" > /dev/null 2>&1; then
	echo "[qemu_userland_test] FAT file deleted by the guest is still there"
	status=1
fi
ext_keep=$(debugfs -R "cat /$EXT_SCRATCH/$WRITE_KEEP_NAME" "$EXT_PART" 2> /dev/null)
if [ "$ext_keep" != "$WRITE_KEEP_TEXT" ]; then
	echo "[qemu_userland_test] ext2 file written by the guest reads back as: $ext_keep"
	status=1
fi
if debugfs -R "stat /$EXT_SCRATCH/$WRITE_GONE_NAME" "$EXT_PART" 2>&1 | grep -q "Inode:"; then
	echo "[qemu_userland_test] ext2 file deleted by the guest is still there"
	status=1
fi
c_fat=$(mtype -i "$FAT_PART" "::/$FAT_SCRATCH/$C_FILE_NAME" 2>&1)
c_ext=$(debugfs -R "cat /$EXT_SCRATCH/$C_FILE_NAME" "$EXT_PART" 2> /dev/null)
if [ "$c_fat" != "$C_FILE_TEXT" ] || [ "$c_ext" != "$C_FILE_TEXT" ]; then
	echo "[qemu_userland_test] files written through the C library read back as: $c_fat | $c_ext"
	status=1
fi
if mtype -i "$FAT_PART" "::/$FAT_SCRATCH/$C_GONE_NAME" > /dev/null 2>&1; then
	echo "[qemu_userland_test] FAT file deleted through the C library is still there"
	status=1
fi
if debugfs -R "stat /$EXT_SCRATCH/$C_GONE_NAME" "$EXT_PART" 2>&1 | grep -q "Inode:"; then
	echo "[qemu_userland_test] ext2 file deleted through the C library is still there"
	status=1
fi
fat_tree=$(mtype -i "$FAT_PART" "::/$FAT_SCRATCH/$WRITE_TREE_DIR/$WRITE_TREE_NAME" 2>&1)
ext_tree=$(debugfs -R "cat /$EXT_SCRATCH/$WRITE_TREE_DIR/$WRITE_TREE_NAME" "$EXT_PART" 2> /dev/null)
if [ "$fat_tree" != "$WRITE_TREE_TEXT" ] || [ "$ext_tree" != "$WRITE_TREE_TEXT" ]; then
	echo "[qemu_userland_test] a file moved into a new directory reads back as: $fat_tree | $ext_tree"
	status=1
fi
fat_long=$(mtype -i "$FAT_PART" "::/$FAT_SCRATCH/$WRITE_LONG_NAME" 2>&1)
ext_long=$(debugfs -R "cat \"/$EXT_SCRATCH/$WRITE_LONG_NAME\"" "$EXT_PART" 2> /dev/null)
if [ "$fat_long" != "$WRITE_LONG_TEXT" ] || [ "$ext_long" != "$WRITE_LONG_TEXT" ]; then
	echo "[qemu_userland_test] a file with a long name reads back as: $fat_long | $ext_long"
	status=1
fi
fat_listing=$(mdir -i "$FAT_PART" "::/$FAT_SCRATCH/$WRITE_KEEP_NAME" 2>&1)
if ! echo "$fat_listing" | grep -q "$(date -u +%Y-%m)"; then
	echo "[qemu_userland_test] FAT file has no current date: $fat_listing"
	status=1
fi
ext_times=$(debugfs -R "stat /$EXT_SCRATCH/$WRITE_KEEP_NAME" "$EXT_PART" 2> /dev/null | grep "mtime:")
if ! echo "$ext_times" | grep -q "$(date -u +%Y)"; then
	echo "[qemu_userland_test] ext2 file has no current modification time: $ext_times"
	status=1
fi
rm -rf "$PART_DIR"

if grep -q "Exception{" "$SERIAL_FILE"; then
	echo "[qemu_userland_test] a thread was killed by an exception"
	status=1
fi

if [ "$status" -eq 0 ]; then echo "[qemu_userland_test] userland works end to end"; fi
exit "$status"
