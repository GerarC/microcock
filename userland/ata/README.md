# ata

The block server. It drives the first IDE disk with programmed I/O and serves sectors, and the partitions it finds, to other programs by IPC and shared memory. The file system servers `fat` and `ext` are its clients.

## Where it sits

```text
fat, ext, blkclient --BLK_*--> ata --port I/O--> primary channel, master drive
```

- Capabilities, from `initrd/manifest.conf`: `io:0x1F0+8` (command ports) and `io:0x3F6+1` (control port). Nothing else. The manifest line is `ata path=bin/ata.elf` with those grants; the test manifest adds only `blkclient`, which has no capabilities.
- `init` starts it before `fat` and `ext` and sends `CAPABILITY_GRANTED`. Requests that arrive before the grant are kept (up to 16) and answered after the drive is ready.
- It talks only to the master of the primary channel. Under QEMU the ISO is the master of the secondary channel, so the two do not clash.

## Protocol

Header: `shared/include/cock/abi/block.hpp`; partition parsers: `shared/include/cock/abi/partition.hpp`. Every request carries a request id in `arg0`; the reply `BLK_REPLY` echoes it in `arg0`, puts the status in `arg1` and up to three values in `arg2` to `arg4`.

| Message | Arguments | Reply values |
|---------|-----------|--------------|
| `BLK_CONNECT` | none | id of the shared window, number of devices |
| `BLK_INFO` | `arg1` device | sector count, first sector on the disk, kind |
| `BLK_READ` | `arg1` device, `arg2` sector, `arg3` count | sectors copied into the window |
| `BLK_WRITE` | `arg1` device, `arg2` sector, `arg3` count; data in the window | sectors written |

Device `DISK_DEVICE` is the whole disk; partitions follow in table order (`MAX_PARTITIONS`, 16). Kinds are `DISK`, `MBR_PARTITION` and `GPT_PARTITION`. Statuses: `OK`, `NOT_CONNECTED`, `BAD_REQUEST`, `BAD_DEVICE`, `OUT_OF_RANGE`, `IO_ERROR`, `NO_SPACE`, `READ_ONLY`.

Sectors are relative to the device, so a file system sees its partition from sector 0. The server checks the device index, a count of 0 or above `MAX_SECTORS_PER_READ` (one window), and that `sector + count` fits without wrapping before it touches the disk. `BLK_WRITE` on the whole disk answers `READ_ONLY`, so partition tables and the boot sector cannot be reached.

On `BLK_CONNECT` the server creates a one page shared memory object, shares it with the client and replies with its id. It keeps up to 8 sessions keyed by the badge of the message (the process id of the client, by the convention of `names`; the window is shared by process id) and frees those of dead clients when the table is full. The client library is `userland/lib/include/cock/user/block.hpp` (`cock::user::block::Client`).

## Driver behaviour

- Programmed I/O only (`rep insw` and `rep outsw`), so the controller cannot write into memory the driver does not own. No bus-master DMA.
- Polling with bounded waits; the interrupt is disabled with `nIEN`. A drive that stops answering gives `IO_ERROR`.
- `IDENTIFY DEVICE` detects the drive; an ATAPI device or no drive prints `ata: no drive` and exits. LBA28 only.
- Writes end with `CACHE FLUSH`. MBR is read from sector 0 (extended partitions are not followed); a protective entry leads to GPT with header and array CRC checks. A bad GPT prints `ata: GPT rejected` and the disk is still served as device 0.
- On success it prints `ata: <sectors> sectors, <devices> devices`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: `waitForCapabilities`, opens the drive, scans the partitions, drains the kept messages, then the request loop |
| `src/grant.cpp`, `include/ata/grant.hpp` | `waitForCapabilities`: blocks until `CAPABILITY_GRANTED`, keeping every other message |
| `src/dispatch.cpp` | `handle`: ignores messages with no reply token and refuses requests without a session, then calls one handler |
| `src/devices.cpp`, `include/ata/devices.hpp` | The drive, the table of devices and `scanPartitions` (MBR and GPT) |
| `src/sessions.cpp`, `include/ata/sessions.hpp` | The sessions of the clients: `connect`, `releaseDeadSessions` |
| `src/requests.cpp`, `include/ata/requests.hpp` | `answerInfo`, `answerRead`, `answerWrite` |
| `src/pending.cpp`, `include/ata/pending.hpp` | `keepPending` and `handlePending`: requests that arrive before the grant |
| `src/reply.cpp`, `include/ata/reply.hpp` | `reply`: sends `BLK_REPLY` |
| `include/ata/drive.hpp` | `cock::ata::Drive`: port registers, `init`, `read`, `write`, status polling; header only |
| `Makefile` | `NAME = ata`, `SRCS` under `src/` and the small `LIBC_SRCS` list |

## Build, run, test

```sh
make userland/ata
make disk
make run-headless DISK=build/disk.img
bash scripts/make_test_disk.sh gpt build/test_disk_gpt.img
DISK_SCHEME=gpt bash scripts/qemu_userland_test.sh
```

Use `HOST_TOOLCHAIN=1` without the cross compiler. `make disk` takes `DISK_SCHEME=mbr|gpt`. `scripts/qemu_userland_test.sh` builds the test disk (`scripts/make_test_disk.sh`, layout in `scripts/test_disk_layout.sh`), attaches it and requires the `ata:` line, the devices listed by `blkclient` and the marker sector read back. Run it with both schemes. The partition parsers have host tests in `test/host/test_partition.cpp`.

## Limits and known gaps

- Access control is by IPC capability: only `fat`, `ext` and the test client `blkclient` hold one for `ata`; a holder can read the whole disk and write any partition.
- No DMA and no IRQ driven I/O; a DMA driver needs the DMA policy first.
- One drive, 28 bit sector numbers (128 GiB), logical partitions inside an extended partition and the backup GPT are not read.

## Documentation

- [Block storage](../../documentation/userland/block_storage.md)
- [FAT32 server](../../documentation/userland/fat32.md)
- [ext2 server](../../documentation/userland/ext2.md)
- [Capabilities](../../documentation/core/capabilities.md)
