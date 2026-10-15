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
		LINUX_TYPE=83
		;;
	gpt)
		LABEL=gpt
		FIRST_TYPE=L
		SECOND_TYPE=U
		LINUX_TYPE=L
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
start=$THIRD_START, size=$THIRD_SECTORS, type=$LINUX_TYPE
start=$FOURTH_START, size=$FOURTH_SECTORS, type=$LINUX_TYPE
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

EXT_TREE="$WORK/ext"
mkdir -p "$EXT_TREE/subdir" "$EXT_TREE/many"
echo "$EXT_SMALL_TEXT" > "$EXT_TREE/small.txt"
echo "$EXT_INNER_TEXT" > "$EXT_TREE/subdir/inner.txt"
yes "$FAT_BIG_LETTERS" | tr -d '\n' | head -c "$EXT_LARGE_BYTES" > "$EXT_TREE/large.bin"
printf '%s' "$EXT_SPARSE_HEAD" > "$EXT_TREE/sparse.bin"
truncate -s "$EXT_SPARSE_BYTES" "$EXT_TREE/sparse.bin"
printf '%s' "$EXT_SPARSE_TAIL" | dd of="$EXT_TREE/sparse.bin" bs=1 seek=$((EXT_SPARSE_BYTES - ${#EXT_SPARSE_TAIL})) conv=notrunc status=none
: > "$EXT_TREE/empty.txt"
for number in $(seq 1 "$EXT_MANY_FILES"); do
	: > "$EXT_TREE/many/$(printf 'file%03d' "$number")"
done
ln -s small.txt "$EXT_TREE/link-fast"
ln -s "$EXT_LONG_LINK_TARGET" "$EXT_TREE/link-slow"
ln -s /subdir/inner.txt "$EXT_TREE/link-abs"
ln -s subdir/inner.txt "$EXT_TREE/link-sub"
ln -s loop-b "$EXT_TREE/loop-a"
ln -s loop-a "$EXT_TREE/loop-b"

EXT_SIZE_KIB=$((THIRD_SECTORS * SECTOR_BYTES / EXT_BLOCK_BYTES))
mke2fs -q -t ext2 -b "$EXT_BLOCK_BYTES" -d "$EXT_TREE" -E "offset=$((THIRD_START * SECTOR_BYTES))" "$OUTPUT" "$EXT_SIZE_KIB"

EXT4_SIZE_KIB=$((FOURTH_SECTORS * SECTOR_BYTES / EXT_BLOCK_BYTES))
mke2fs -q -t ext4 -b "$EXT_BLOCK_BYTES" -E "offset=$((FOURTH_START * SECTOR_BYTES))" "$OUTPUT" "$EXT4_SIZE_KIB"
