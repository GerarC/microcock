#!/usr/bin/env bash
set -u

BOOT_SECONDS="${BOOT_SECONDS:-6}"
KEY_SECONDS="${KEY_SECONDS:-2}"
CLIENTS_TIMEOUT="${CLIENTS_TIMEOUT:-60}"
POLL_SECONDS=1
CLIENTS_DONE_TEXT=("fatclient: ok" "extclient: ok" "cfile: ok" "pipeclient: ok" "modeprobe: ok" "descprobe: ok" "spawner: ok" "capreceiver: ok" "capgiver: ok" "epserver: ok" "epgiver: ok" "muteclient: ok" "phoenixclient: ok" "pmclient: ok" "forkprobe: ok" "sigprobe: ok" "execchild: ok" "rs: crasher gave up after 3 restarts" "syncbench: ok async" "schedbench: done")
SERIAL_FILE="${SERIAL_FILE:-build/userland_serial.txt}"
SOCKET_FILE="${SOCKET_FILE:-build/userland_monitor.sock}"
QEMU_BIN="${QEMU:-qemu-system-i386}"
SHELL_COMMANDS="echo hello shell|uname|ps|ls /|cat /mounts.conf|ls /ext|write /ext/scratch/sh.txt written by sh|cat /ext/scratch/sh.txt|mv /ext/scratch/sh.txt /ext/scratch/moved.txt|cat /ext/scratch/moved.txt|rm /ext/scratch/moved.txt|cat /ext/scratch/moved.txt|mkdir /ext/scratch/made|ls /ext/scratch|rmdir /ext/scratch/made|rmdir /ext/scratch/made|cd /ext/scratch|pwd|write rel.txt relative ok|cat rel.txt|cat ../scratch/./rel.txt|rm rel.txt|cd ..|pwd|cd /nowhere|cd /mounts.conf|cd|pwd|echo 'single   quoted'|echo \"double   quoted\" end|echo 'open|exitwith 7|nosuchprogram|/bin/exitwith 0|cd /bin|./exitwith 5|echo st=\$?|echo in \$PWD|cd|NAME=\"a  b\"|echo var:\$NAME|unset NAME|echo gone:\$NAME:|sleep 1|clear|echo after clear"
SHELL_OUTPUT_COPIES=2
SECOND_COMMAND="echo on second"
TERMINAL_MARK="=====SECOND TERMINAL====="
SECOND_SCREEN_TEXT="cock> echo on second|on second"
FILE_CLIENT_RUNS=10
CLOCK_FALLBACK_YEAR=2001
DISK_SCHEME="${DISK_SCHEME:-mbr}"
DISK_FILE="${DISK_FILE:-build/test_disk_$DISK_SCHEME.img}"
EXPECTED_SCREEN_TEXT="cock> echo after clear|after clear"
PROJECT_VERSION="$(sed -n "s/^VERSION_MAJOR := //p" Makefile).$(sed -n "s/^VERSION_MINOR := //p" Makefile).$(sed -n "s/^VERSION_PATCH := //p" Makefile)"
EXPECTED_SERIAL_TEXT="Hello from cock ring 3!!!|probe: 64/64 checks passed|microcock v${PROJECT_VERSION} shell, type help|hello shell|PID PPID STAT|manifest.conf|mounts.conf|/ext ext|subdir/|written by sh|cat: /ext/scratch/moved.txt: not found|made/|rmdir: /ext/scratch/made: not found|cock> /ext/scratch|relative ok|sh: cd: /nowhere: not found|sh: cd: /mounts.conf: not a directory|single   quoted|double   quoted end|sh: unterminated quote|exitwith: 2 arguments, first exitwith|sh: exitwith: exit status 7|sh: nosuchprogram: command not found|exitwith: 2 arguments, first /bin/exitwith|exitwith: 2 arguments, first ./exitwith|sh: ./exitwith: exit status 5|st=5|in /bin|var:a  b|gone::|abort()|pipeclient: ok|modeprobe: ok|descprobe: ok|probe: printf -42 4294967295 FF hi Z 0xBEEF|probe: initrd says Hello from the cock initrd!!!|rs: initrd is boot module|Boot memory released|rs: manifest has 47 entries|rs: restarted phoenix 2 of 5|contruncated: cut|rs: configured vga_driver|rs: started console|console: ready|rs: started tty|tty: ready|rs: started kbd|pm: ready|rs: started pm|rs: started init|init: ready|rs: started sh for init|init: started sh|rs: started ata|rs: started rtc|rtc: ready|rs: started fat|rs: started vfs|vfs: 4 mounts|rs: fsclient exit notices 10/10|pmclient: ok|forkprobe: ok|sigprobe: ok|execprobe: bad exec refused, image intact|execchild: ok|rs: started probe|kbd: ready|Screen mapped by PID|reaped"

. "$(dirname "$0")/test_disk_layout.sh"
case "$DISK_SCHEME" in
	gpt) PARTITION_KIND=$KIND_GPT_PARTITION ;;
	*) PARTITION_KIND=$KIND_MBR_PARTITION ;;
esac
EXPECTED_SERIAL_TEXT="$EXPECTED_SERIAL_TEXT|ata: $DISK_SECTORS sectors, $DEVICE_COUNT devices|blkclient: device 0: kind $KIND_DISK first $DISK_FIRST_SECTOR sectors $DISK_SECTORS|blkclient: device 1: kind $PARTITION_KIND first $FIRST_START sectors $FIRST_SECTORS|blkclient: device 2: kind $PARTITION_KIND first $SECOND_START sectors $SECOND_SECTORS|blkclient: sector says $MARKER_TEXT|blkclient: device 3: kind $PARTITION_KIND first $THIRD_START sectors $THIRD_SECTORS|blkclient: device 4: kind $PARTITION_KIND first $FOURTH_START sectors $FOURTH_SECTORS|blkclient: ok|fat: device $FAT_DEVICE,|fatclient: hello says $FAT_HELLO_TEXT|fatclient: ok|ext: device $EXT_DEVICE,|ext: device $EXT4_DEVICE: unsupported feature extents|extclient: small says $EXT_SMALL_TEXT|extclient: ok|muteclient: dead server did not hang the vfs|muteclient: ok|cfile: ext ok|cfile: fat ok|cfile: directories ok|cfile: ok|spawner: forged label refused|spawner: loader mistakes refused|spawner: spawned a program|spawner: ok|epgiver: endpoint bound to the server|epgiver: the server saw the badge of the handle|epgiver: nothing works without a handle|epgiver: a request was forwarded and answered through its token|epgiver: tokens nobody gave and spent tokens fail|epgiver: a requester cannot open more tokens than its quota|epgiver: the kernel replaced the forged badges|epgiver: a revoked handle leads nowhere|epgiver: binding again moved the endpoint"

make iso TEST_INITRD=1 "$@" || exit 2

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

screens=$(python3 - "$SOCKET_FILE" "$SHELL_COMMANDS" "$KEY_SECONDS" "$SECOND_COMMAND" "$TERMINAL_MARK" <<'PYTHON'
import re
import socket
import sys
import time

socket_path, commands, key_seconds = sys.argv[1], sys.argv[2].split("|"), float(sys.argv[3])
second_command, terminal_mark = sys.argv[4], sys.argv[5]
KEY_NAMES = {" ": "spc", "/": "slash", ".": "dot", "-": "minus", "'": "apostrophe", "\"": "shift-apostrophe", "$": "shift-4", "?": "shift-slash", "=": "equal", ":": "shift-semicolon"}
KEY_HOLD_MS = 20
KEY_WAIT = 0.07
ENTER_WAIT = 0.8
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


for line in commands:
    for character in line:
        key = KEY_NAMES.get(character, "shift-" + character.lower() if character.isupper() else character)
        command("sendkey %s %d" % (key, KEY_HOLD_MS), KEY_WAIT)
    command("sendkey ret %d" % KEY_HOLD_MS, ENTER_WAIT)
time.sleep(key_seconds)


def screen_text():
    dump = command("xp/%dbx %s" % (SCREEN_BYTES, VIDEO_ADDRESS), 1.5)
    cells = []
    for line in dump.splitlines():
        match = re.match(r"^[0-9a-f]{8,16}:(.*)$", line.strip())
        if match:
            cells += [int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", match.group(1))]
    characters = cells[0::CELL_BYTES]
    rows = []
    for row in range(ROWS):
        text = "".join(chr(c) if 32 <= c < 127 else " " for c in characters[row * COLUMNS:(row + 1) * COLUMNS])
        rows.append(text.rstrip())
    return "\n".join(rows)


command("sendkey alt-f2 %d" % KEY_HOLD_MS, ENTER_WAIT)
time.sleep(key_seconds)
for character in second_command:
    key = KEY_NAMES.get(character, "shift-" + character.lower() if character.isupper() else character)
    command("sendkey %s %d" % (key, KEY_HOLD_MS), KEY_WAIT)
command("sendkey ret %d" % KEY_HOLD_MS, ENTER_WAIT)
time.sleep(key_seconds)
second = screen_text()
command("sendkey alt-f1 %d" % KEY_HOLD_MS, ENTER_WAIT)
time.sleep(key_seconds)
print(screen_text())
print(terminal_mark)
print(second)
command("quit", 0.2)
PYTHON
)
kill "$qemu_pid" 2> /dev/null
wait "$qemu_pid" 2> /dev/null
screen=${screens%%"$TERMINAL_MARK"*}
second_screen=${screens#*"$TERMINAL_MARK"}

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
IFS='|' read -ra second_checks <<< "$SECOND_SCREEN_TEXT"
for expected in "${second_checks[@]}"; do
	if ! echo "$second_screen" | grep -qF "$expected"; then
		echo "[qemu_userland_test] missing on the second terminal: $expected"
		status=1
	fi
done
if echo "$screen" | grep -qF "on second"; then
	echo "[qemu_userland_test] the first terminal shows what was typed on the second"
	status=1
fi
if echo "$second_screen" | grep -qF "after clear"; then
	echo "[qemu_userland_test] the second terminal shows the text of the first"
	status=1
fi
IFS='|' read -ra serial_checks <<< "$EXPECTED_SERIAL_TEXT"
for expected in "${serial_checks[@]}"; do
	if ! grep -qF "$expected" "$SERIAL_FILE"; then
		echo "[qemu_userland_test] missing on serial: $expected"
		status=1
	fi
done
if ! grep -qE "microcock v${PROJECT_VERSION//./\\.}[[:space:]]*\$" "$SERIAL_FILE"; then
	echo "[qemu_userland_test] the program uname printed nothing"
	status=1
fi
shell_copies=$(grep -c "written by sh" "$SERIAL_FILE")
if [ "$shell_copies" -ne "$SHELL_OUTPUT_COPIES" ]; then
	echo "[qemu_userland_test] the shell printed the written file $shell_copies times, expected $SHELL_OUTPUT_COPIES"
	status=1
fi
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
ext_mtime=$(debugfs -R "stat /$EXT_SCRATCH/$WRITE_KEEP_NAME" "$EXT_PART" 2> /dev/null | sed -n 's/^ *mtime: .*-- //p')
case "$ext_mtime" in
	"" | *"$CLOCK_FALLBACK_YEAR"*)
		echo "[qemu_userland_test] ext2 file written by the guest has the fallback time ($ext_mtime): the rtc server did not answer"
		status=1
		;;
esac
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
