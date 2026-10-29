# Testing

* `bash scripts/qemu_userland_test.sh` boots the ISO under QEMU, types `abCd` on the simulated keyboard through the QEMU monitor and requires: the `hello` line and `probe: 54/54 checks passed` on the screen, the typed text on the screen, the lines of the block driver and its client (see [block storage](../drivers/block_storage.md#tests); the script attaches a test disk and accepts `DISK_SCHEME=gpt`), the `printf` sample line `probe: printf -42 4294967295 FF hi Z 0xBEEF` and the `abort()` message of `aborter` on the serial log, `kbd: ready`, the VGA driver detection and the reaping of the processes that exit on the serial log, and not one exception.
* Run it also with `USER_EXTRA_FLAGS=-fno-omit-frame-pointer`, which changes how the compiler addresses locals and caught a real bug in the system call wrapper.
* Both variants run in CI, on every push.
* `bash scripts/host_test.sh` builds `test/host/` with the host compiler and runs it; it needs no emulator and checks the ring buffer, the intrusive queue, the linked list, the message layout, the capability table, the manifest parser, the mount table parser, the partition table parsers, the FAT32 parser and the ext2 parser.
