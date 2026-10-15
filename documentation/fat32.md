# FAT32

`fat` is a Ring 3 server that reads a FAT32 volume of the disk and answers the same protocol as `ramfs` (see [file system](filesystem.md)), so a client reaches it through the `vfs` as `/fat/...` with the same client library. It talks to the disk through the block protocol of [block storage](block_storage.md), as any other client of `ata`, and has no capability of its own. It is read only.

## Mounting
`fat` waits for the message `init` sends it, whose second argument is the block device to serve: the manifest line is `fat path=bin/fat.elf arg=2` (device 0 is the whole disk, the partitions follow). It connects to `ata`, reads the first sector of that device and parses it as a FAT32 boot sector. If the device does not exist or is not FAT32 it prints `fat: device N is not a FAT32 volume` and exits. A line without `arg=` leaves the server waiting for it.

The boot sector (`shared/include/cock/abi/fat32.hpp`, `parseBpb`, covered by the host tests) is accepted when:
- the signature `0x55 0xAA` is at byte 510 and a sector is 512 bytes (other sector sizes are refused);
- the sectors per cluster are a power of two up to 128, there are 1 to 4 FATs and at least one reserved sector;
- the root entry count and the 16-bit FAT size are zero and the 32-bit FAT size is not (the usual sign of FAT32), and the version is 0;
- the total sector count fits in the partition, the reserved sectors and the FATs leave room for data, and the cluster count is between 65525 and `0x0FFFFFF4` (a smaller count is FAT12 or FAT16 whatever the label says);
- the FAT is big enough for that many clusters and the root cluster is a data cluster.

Only the first FAT is read; the mirror and the FSInfo sector are ignored because nothing is written.

## What it serves
- **Paths.** `/`, `a/b` and `/a/b/` are the same; a component is looked up in its directory with the case ignored, against the long name when the entry has one and the short name otherwise. `.` and `..` are not listed and not followed by name.
- **Names.** Long file names (up to 255 UTF-16 units) are assembled from their entries and used only if the sequence is complete and its checksum matches the short name; otherwise the short name is shown. They are turned into UTF-8 (surrogate pairs become one four-byte character, a lone surrogate becomes `?`), so a client opens `/fat/café.txt` by its UTF-8 name. Matching ignores the case of ASCII letters only; other characters must match byte for byte. Short names use the lower-case flags of the entry (`hello.txt`) and their bytes beyond ASCII are passed as they are.
- **Files.** `FS_READ` follows the cluster chain from the cluster of the file. Each open handle remembers the last cluster it reached, so a sequential read does not walk the chain again. A read that starts on a sector and covers whole sectors inside a cluster goes to the disk in one request, straight into the window, up to the 8 sectors a window holds; the rest goes through a one sector buffer.
- **Directories.** `FS_READDIR` lists the entries in disk order (deleted entries, the volume label and the dot entries are skipped), `OUT_OF_RANGE` after the last one. A directory that cannot be read to the end also answers `OUT_OF_RANGE`.
- **Sessions and handles** are as in `ramfs`: 8 clients, 16 handles each, windows released for dead clients.

## Hostile images
The server never trusts the disk. Every cluster number read from the FAT or from a directory entry must be a data cluster of the volume before it is followed; a chain that ends early or points outside makes the read fail with `IO_ERROR`; a directory walk stops after as many clusters as the volume has, so a loop in a chain ends; the offset and length of a read are clamped to the size in the entry, so a size larger than the chain gives `IO_ERROR`, not a read past the data.

## Limits
Read only; 512 byte sectors; the first FAT only; one volume; case-insensitive matching for ASCII only. Writing is the next step of the plan.

## Tests
- Host tests (`host_test/test_fat32.cpp`): the boot sector (a good one and ten refusals), FAT entries and end of chain, short names with case flags and the kanji escape, skipped entries, long names with a wrong checksum and with a missing part, and the UTF-8 conversion of two, three and four byte characters, a lone high surrogate and a lone low surrogate.
- End to end: `scripts/make_test_disk.sh` puts these files in the FAT32 partition: `HELLO.TXT` (`hello from fat`), the same text under the long name `A long file name.txt`, `SUBDIR/NESTED.TXT`, and `BIG.BIN` with 20000 bytes of the letters `A` to `Z` repeating (many clusters, because the test volume uses one sector per cluster). `fatclient` reads each through `/fat/...` with other capitalization where it matters, reads the big file in windows and checks every byte, reads across a sector boundary, reads a file whose long name has non-ASCII characters (`café ñ.txt`), lists `/fat` (five entries, with the right types), checks that a missing file and a path through a file are refused, and checks that `/hello.txt` of the initrd still reads. The test requires `fat: device 2,`, `vfs: 2 mounts`, `fatclient: hello says hello from fat` and `fatclient: ok`, with an MBR disk and with `DISK_SCHEME=gpt`.
- The probe checks of the file server now run through the `vfs`, so the refusals (bad handle, bad path, bad flags, reads past the end) are also checks of the proxy.
