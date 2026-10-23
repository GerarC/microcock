# Block storage

The disk driver is not in the kernel. `ata` is a Ring 3 server that reads sectors from the first IDE disk with programmed I/O and serves them, together with the partitions it finds, to other programs by IPC and shared memory. File systems for the disk (FAT32, ext2) are built on top of it.

## The driver
`ata` talks to the primary channel, master drive: command ports `0x1F0` to `0x1F7` and control port `0x3F6`. It needs two `IO_PORTS` capabilities, and nothing else: the manifest line is

```
ata path=bin/ata.elf caps=io:0x1F0+8,io:0x3F6+1
```

- **Programmed I/O only.** The data comes through the data port with `rep insw`. There is no bus-master DMA, so the driver cannot make the controller write into memory it does not own, and the capability model stays sound. A DMA-capable driver would hold as much authority as the root server and needs the DMA policy of the capability stage first.
- **Polling, no IRQ.** At start the driver sets `nIEN` in the control register, so the drive never raises IRQ 14, and waits for `BSY` to clear. Every wait is bounded by a poll limit, so a drive that stops answering makes the request fail with `IO_ERROR` instead of hanging the server. The IRQ path belongs with the timers of a later stage.
- **Detection.** `IDENTIFY DEVICE` tells whether a drive is there (status `0x00` or `0xFF` means no, a drive that answers on the cylinder registers is ATAPI and is refused) and gives the sector count. If nothing answers the program prints `ata: no drive` and exits.
- **LBA28 only.** A sector number is 28 bits, so the driver sees at most 128 GiB; a bigger disk is cut to that size. Reads use `READ SECTORS` (`0x20`). Writes use `WRITE SECTORS` (`0x30`): the driver waits for `DRQ` before each sector, sends it with `rep outsw`, waits for `BSY` to clear, checks `ERR` and `DF`, and ends with `CACHE FLUSH` (`0xE7`), so a write that was acknowledged has reached the device.
- Only the master of the primary channel is driven. The ISO is the master of the secondary channel under QEMU, so it does not clash.

## Partitions
After the capability grant the driver reads sector 0 and builds a table of devices, then answers requests. Device 0 is the whole disk; the partitions follow in table order, at most 16.

- **MBR.** The signature `0x55 0xAA` at bytes 510 and 511, four entries of 16 bytes from byte 446. An entry is kept when its type is not empty, not an extended partition (`0x05`, `0x0F`, `0x85`) and its range lies inside the disk. Logical partitions inside an extended partition are not read yet.
- **GPT.** A table with a protective entry (type `0xEE`) sends the driver to LBA 1. The header must have the `EFI PART` signature, a size between 92 and 512 bytes, a valid header CRC32, `MyLBA` equal to 1, 128 byte entries (at most 128 of them) and an entry array that lies inside the disk; the array CRC32 must match too. An entry is kept when its type GUID is not zero and its range is ordered and inside the usable range of the header. Only the primary header is used; the backup copy is not read.
- A bad table never stops the driver: the disk stays served as device 0 and, for GPT, the serial log shows `ata: GPT rejected`.

The parsers are in `shared/include/cock/abi/partition.hpp`, which has no dependency on the driver and is covered by the host tests (`test/host/test_partition.cpp`): a good table of each kind, entries outside the disk, extended partitions, a corrupted header, a corrupted entry array and an entry array placed past the end of the disk.

## Protocol
Requests and replies are IPC messages. Every request carries a request id in `arg0`; the reply (`BLK_REPLY`) echoes it in `arg0` and carries the status in `arg1` and up to three values in `arg2` to `arg4`. A client accepts a reply only from the server pid and with its own id. The constants are in `shared/include/cock/abi/block.hpp`.

| Message | Arguments | Reply values |
|---------|-----------|--------------|
| `BLK_CONNECT` | none | id of the window, number of devices |
| `BLK_INFO` | `arg1` device | sector count, first sector on the disk, kind |
| `BLK_READ` | `arg1` device, `arg2` first sector, `arg3` sector count | sectors copied into the window |
| `BLK_WRITE` | `arg1` device, `arg2` first sector, `arg3` sector count (the data is in the window) | sectors written |

Kinds: `DISK` (0), `MBR_PARTITION` (1), `GPT_PARTITION` (2). Statuses: `OK`, `NOT_CONNECTED`, `BAD_REQUEST`, `BAD_DEVICE`, `OUT_OF_RANGE`, `IO_ERROR`, `NO_SPACE`, `READ_ONLY`.

`BLK_WRITE` is refused with `READ_ONLY` on device 0, the whole disk: a client can write only inside a partition, so the partition tables and the boot sector cannot be reached. Checks on the device, count and range are the same as for `BLK_READ`.

The sector in `BLK_READ` is relative to the device, so a file system server sees its partition from sector 0 and cannot reach the neighbors. The driver checks the device index, a count of 0 or more than the window holds (8 sectors), and that `sector + count` fits in the device without wrapping, before it touches the disk.

### Window and sessions
Same design as the file server: on `BLK_CONNECT` the **server** creates a one page shared memory object, shares it with the client with write access, maps it and replies with its id; the data of `BLK_READ` goes to the start of the window. The server keeps up to 8 sessions keyed by the kernel stamped `sender_pid`; when the table is full it asks `SYS_PROC_ALIVE` about every session and frees the ones of dead clients. Requests that arrive before the capability grant are kept (up to 16) and answered afterwards.

Only a process that holds an IPC capability for `ata`, or that `ata` holds one for, can talk to the block server (see the IPC capability in [capabilities](../core/capabilities.md)): in the shipped manifest that is `fat` and `ext`. A holder can still read the whole disk and write any partition, so who gets the capability is the policy.

## Client library
`userland/lib/include/cock/user/block.hpp` offers `cock::user::block::Client` with `connect`, `deviceCount`, `info`, `read` and `write`. A program that uses it lists `string/memcpy.cpp` and `string/strlen.cpp` in `LIBC_SRCS`. `api.hpp` has `port_out8`, `port_read_words` and `port_write_words` for the driver.

## Tests
- `bash scripts/make_test_disk.sh mbr|gpt <file>` builds a 63 MiB sparse image with four partitions (`sfdisk`, no root), writes a marker text in the first sector of the first partition, formats the second one as FAT32 with `mformat` and `mcopy` from mtools (a FAT32 volume needs at least 65525 clusters, so it is 40 MiB; see [FAT32](fat32.md#tests)), and builds an ext2 volume in the third and an ext4 volume in the fourth with `mke2fs` (see [ext2](ext2.md#tests)). The layout constants are in `scripts/test_disk_layout.sh`, which the end to end test sources too.
- `scripts/qemu_userland_test.sh` attaches the image as the primary master (`DISK_SCHEME=mbr`, the default, or `gpt`) and boots from the ISO. It requires the driver line `ata: 129024 sectors, 5 devices`, the devices that `blkclient` lists with their kind, first sector and size, and the marker text read back through the protocol (`blkclient: sector says cock block marker`).
- `blkclient` is the test client: `init` starts it after `ata`. Run the end to end test with `DISK_SCHEME=gpt` as well as the default. CI needs `sfdisk`, which comes with `util-linux`.
