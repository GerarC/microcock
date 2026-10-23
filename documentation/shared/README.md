# Shared headers

`shared/include/cock/` is included by the kernel and by every program. A number or a layout changed here changes both sides, so both are checked together and `static_assert` pins every place where the two must agree.

| Folder | Holds |
|--------|-------|
| `abi/` | Wire formats and constants: capabilities, processes, shared memory, synchronous calls, the manifest and mounts, the block, console, file system, FAT32, ext2, RTC and date-time protocols |
| `ipc/` | The message layout, the message types and `KERNEL_SENDER` (see [ipc](../core/ipc.md)) |
| `data_structure/` | Freestanding containers: intrusive queue, linked list, ring buffer, range pool |
| `shell/` | The line splitter and number parser of the shell, host tested |

Each header documents its fields and ranges as Doxygen comments. How the protocols are used: [userland](../userland/README.md). The tests that run these headers on the host live in `test/host/`.
