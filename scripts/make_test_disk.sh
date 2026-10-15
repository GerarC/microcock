#!/usr/bin/env bash
set -eu

KIND="${1:?partition scheme: mbr or gpt}"
OUTPUT="${2:?output image}"

. "$(dirname "$0")/test_disk_layout.sh"

case "$KIND" in
	mbr)
		LABEL=dos
		FIRST_TYPE=83
		SECOND_TYPE=0b
		;;
	gpt)
		LABEL=gpt
		FIRST_TYPE=L
		SECOND_TYPE=U
		;;
	*)
		echo "[make_test_disk] unknown scheme: $KIND"
		exit 2
		;;
esac

mkdir -p "$(dirname "$OUTPUT")"
rm -f "$OUTPUT"
truncate -s $((DISK_SECTORS * SECTOR_BYTES)) "$OUTPUT"

sfdisk --quiet --no-reread "$OUTPUT" <<LAYOUT
label: $LABEL
start=$FIRST_START, size=$FIRST_SECTORS, type=$FIRST_TYPE
start=$SECOND_START, size=$SECOND_SECTORS, type=$SECOND_TYPE
LAYOUT

printf '%s' "$MARKER_TEXT" | dd of="$OUTPUT" bs=$SECTOR_BYTES seek=$FIRST_START conv=notrunc status=none

FAT_VOLUME="$OUTPUT@@$((SECOND_START * SECTOR_BYTES))"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "$FAT_HELLO_TEXT" > "$WORK/hello.txt"
echo "$FAT_NESTED_TEXT" > "$WORK/nested.txt"
yes "$FAT_BIG_LETTERS" | tr -d '\n' | head -c "$FAT_BIG_BYTES" > "$WORK/big.bin"

export LC_ALL=C.UTF-8
mformat -i "$FAT_VOLUME" -F -T "$SECOND_SECTORS" -c "$FAT_CLUSTER_SECTORS" ::
mmd -i "$FAT_VOLUME" "::/$FAT_DIRECTORY"
mcopy -i "$FAT_VOLUME" "$WORK/hello.txt" ::/HELLO.TXT
mcopy -i "$FAT_VOLUME" "$WORK/hello.txt" "::/$FAT_LONG_NAME"
mcopy -i "$FAT_VOLUME" "$WORK/hello.txt" "::/$FAT_UNICODE_NAME"
mcopy -i "$FAT_VOLUME" "$WORK/big.bin" ::/BIG.BIN
mcopy -i "$FAT_VOLUME" "$WORK/nested.txt" "::/$FAT_DIRECTORY/NESTED.TXT"
