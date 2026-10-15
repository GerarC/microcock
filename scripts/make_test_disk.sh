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
