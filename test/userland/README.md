# test/userland

Programs that exist only to test the system from Ring 3. The normal initrd never contains them: they are built and packed only with `TEST_INITRD=1`, which `scripts/qemu_userland_test.sh` sets. Each folder has a `Makefile` that includes `../../../userland/common.mk`, and the root `Makefile` builds them into `build/userland` next to the real programs.

## Where it sits

- `test/initrd/manifest.conf` is appended to `initrd/manifest.conf` (system lines first) and `test/initrd/mounts.conf` to `initrd/mounts.conf`. `init` therefore starts the tests after the system servers, in file order. None of them gets a capability.
- The test initrd also holds the fixtures `hello.txt` and `docs/note.txt`, and the mount `/mute mute`.
- `ext4probe` is not a program of this folder: it is `ext.elf` started a second time with `arg=4`, the ext4 volume of the test disk, which `ext` must refuse.
- `fuzz` is started by the manifest only with `FUZZ=user`, or replaces `init` with `FUZZ=root`.

## Programs

| Program | What it checks |
|---------|----------------|
| `hello` | Assembly (`main.s`): `SYS_WRITE` and `SYS_EXIT` work; prints `Hello from cock ring 3!!!` |
| `probe` | 54 checks from Ring 3, one file per topic: `security.cpp` (ports, IRQ, grant, physical map, kernel writes and unmaps, boot modules and `spawn` are denied), `memory.cpp` (anonymous maps, quota, region table), `ipc.cpp` (reserved messages refused, try and timed receive, `sleep`, liveness), `output.cpp` (oversized write, `printf`), `heap.cpp` (`malloc`, `realloc`, `calloc`, `operator new`, `malloc` under three threads), `shared_memory.cpp`, `files.cpp` (initrd reads, bad requests, directories, manifest, `SYS_MODULE_INDEX` denied), `threads.cpp` (threads, mutex, futex), `sync.cpp` (synchronous calls, queues, dead server, timeouts, defer). Prints `probe: N/N checks passed` |
| `aborter` | Calls `abort()`: the message and the reaping of the process |
| `contruncated` | Sends text to `console` without the closing chunk and exits: the server must still show it (`contruncated: cut`) |
| `syncbench` | Compares the cycles of an asynchronous and a synchronous IPC round trip between two threads; fails when sync is not faster; prints `syncbench: ok async ... sync ...` |
| `schedbench` | Runs a CPU bound and an I/O bound thread and prints their scheduler figures (`schedbench: done cpu response ... io response ...`); fails when the I/O bound thread uses as much CPU as the other. In a kernel without `SCHED_METRICS=1` it prints `schedbench: done metrics off` and passes |
| `fsclient` | Opens, reads and closes `hello.txt` through `vfs`; `init` runs it ten times with `repeat=10` to check that dead sessions are released (`init: fsclient exit notices 10/10`) |
| `blkclient` | Lists the devices of `ata` with kind, first sector and size and reads the marker sector of the first partition |
| `fatclient` | Reads and lists `/fat` (short and long names, the big file byte by byte, UTF-8 names, refusals) and runs the write check in `SCRATCH` |
| `extclient` | The same on `/ext`: direct and indirect blocks, the sparse file, the four kinds of links, the loop, and the write check in `scratch` |
| `cfile` | The C library only (`open`, `read`, `write`, `lseek`, `close`, `unlink`, `mkdir`, `rmdir`, `rename`, `errno`) on `/ext` and `/fat` |
| `mute`, `muteclient` | `mute` reads one message and exits without replying; `muteclient` checks that a request through `vfs` to it fails instead of hanging, that the file system still answers and that a direct client fails too |
| `phoenix`, `phoenixclient` | `phoenix` (manifest `restart=5`) answers one `SYS_PING` and exits; `phoenixclient` finds it again after each exit, three times, and prints `phoenixclient: ok` when every round is answered by a new process |
| `crasher` | Exits at once with status 7 (manifest `restart=3`): `init` restarts it three times and prints `init: crasher gave up after 3 restarts` |
| `fuzz` | Syscall fuzzer: random and hostile arguments, three multi-thread scenarios, a final sanity check; see `documentation/userland/fuzzer.md` |

`lib/include/cock/user/write_check.hpp` is the write check that `fatclient` and `extclient` share: create and read back, large files in windows, overwrite and append, the exclusivity rules, truncate, delete, directories, long names, renames and hard links.

## How they run

`scripts/qemu_userland_test.sh` cleans, builds `make iso TEST_INITRD=1`, builds a test disk with `scripts/make_test_disk.sh` (`DISK_SCHEME=mbr`, or `gpt`), boots QEMU headless with the serial log in a file, waits for the client lines, then types shell commands through the QEMU monitor. It requires the lines listed in `EXPECTED_SERIAL_TEXT` and `EXPECTED_SCREEN_TEXT`, ten `fsclient: ok` lines, no `Exception{`, and, on the guest partitions, `fsck.fat -n`, `e2fsck -fn` and the files read back with `mtype`, `mdir` and `debugfs`. It ends with `userland works end to end`.

```sh
bash scripts/qemu_userland_test.sh HOST_TOOLCHAIN=1
DISK_SCHEME=gpt bash scripts/qemu_userland_test.sh HOST_TOOLCHAIN=1
USER_EXTRA_FLAGS=-fno-omit-frame-pointer bash scripts/qemu_userland_test.sh HOST_TOOLCHAIN=1
FUZZ=user bash scripts/qemu_fuzz_test.sh HOST_TOOLCHAIN=1
FUZZ=root bash scripts/qemu_fuzz_test.sh HOST_TOOLCHAIN=1
bash scripts/host_test.sh
```

The script needs QEMU, GRUB tools, Python 3, `sfdisk`, mtools, `dosfstools` and e2fsprogs; run it on Linux or WSL. Environment variables read by the script include `BOOT_SECONDS`, `CLIENTS_TIMEOUT`, `SERIAL_FILE` and `DISK_FILE`. To build one program use `make userland/<name> TEST_INITRD=1`.

## Limits and known gaps

- The expected check count of `probe` (54) and the other expected lines are written in `scripts/qemu_userland_test.sh`; add a check there when you add one to a program.
- The tests run with timing waits (`BOOT_SECONDS`), so a very slow host may need larger values.
- The fuzzer does not fuzz the file protocols in a structured way, and in root mode `spawn` only receives garbage images.
- `muteclient` and the fuzzer test a server dying; `phoenix` and `crasher` test `init` restarting a program and its limit. No real server (`ramfs`, `ata`) is restarted in a test.
- Nothing here runs as a kernel suite: those are in `test/cock/` and run with `scripts/qemu_test.sh`.

## Documentation

- [Userland programs and testing](../../documentation/userland/userland.md)
- [File system tests](../../documentation/userland/filesystem.md)
- [Syscall fuzzer](../../documentation/userland/fuzzer.md)
- [Block storage tests](../../documentation/userland/block_storage.md)
- [Console tests](../../documentation/userland/console.md)
