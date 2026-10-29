# test/userland

Programs that exist only to test the system from Ring 3. The normal initrd never contains them: they are built and packed only with `TEST_INITRD=1`, which `scripts/qemu_userland_test.sh` sets. Each folder has a `Makefile` that includes `../../../userland/common.mk`, and the root `Makefile` builds them into `build/userland` next to the real programs.

## Where it sits

- `test/initrd/etc/manifest.conf` is appended to `initrd/etc/manifest.conf` (system lines first) and `test/initrd/etc/mounts.conf` to `initrd/etc/mounts.conf`. `rs` therefore starts the tests after the system servers, in file order. None of them gets a capability.
- The test initrd also holds the fixtures `hello.txt` and `docs/note.txt`, and the mount `/mute mute`.
- `ext4probe` is not a program of this folder: it is `ext` started a second time with `arg=4`, the ext4 volume of the test disk, which `ext` must refuse.
- `fuzz` is started by the manifest only with `FUZZ=user`, or replaces `rs` with `FUZZ=root`.

## Programs

| Program | What it checks |
|---------|----------------|
| `hello` | Assembly (`main.s`): `SYS_WRITE` and `SYS_EXIT` work; prints `Hello from cock ring 3!!!` |
| `probe` | 64 checks from Ring 3, one file per topic: `security.cpp` (ports, IRQ, grant, physical map, kernel writes and unmaps, boot modules, `spawn` and each `SYS_PROC_*` call are denied), `memory.cpp` (anonymous maps, quota, region table), `ipc.cpp` (reserved messages refused, try and timed receive, `sleep`, liveness), `output.cpp` (oversized write, `printf`), `heap.cpp` (`malloc`, `realloc`, `calloc`, `operator new`, `malloc` under three threads), `shared_memory.cpp`, `files.cpp` (initrd reads, bad requests, directories, manifest, `SYS_MODULE_INDEX` denied), `threads.cpp` (threads, mutex, futex), `sync.cpp` (synchronous calls, queues, dead server, timeouts, defer). Prints `probe: N/N checks passed` |
| `aborter` | Calls `abort()`: the message and the reaping of the process |
| `contruncated` | Sends text to `console` without the closing chunk and exits: the server must still show it (`contruncated: cut`) |
| `syncbench` | Compares the cycles of an asynchronous and a synchronous IPC round trip between two threads; fails when sync is not faster; prints `syncbench: ok async ... sync ...` |
| `schedbench` | Runs a CPU bound and an I/O bound thread and prints their scheduler figures (`schedbench: done cpu response ... io response ...`); fails when the I/O bound thread uses as much CPU as the other. In a kernel without `SCHED_METRICS=1` it prints `schedbench: done metrics off` and passes |
| `fsclient` | Opens, reads and closes `hello.txt` through `vfs`; `rs` runs it ten times with `repeat=10` to check that dead sessions are released (`init: fsclient exit notices 10/10`) |
| `blkclient` | Lists the devices of `ata` with kind, first sector and size and reads the marker sector of the first partition |
| `fatclient` | Reads and lists `/fat` (short and long names, the big file byte by byte, UTF-8 names, refusals) and runs the write check in `SCRATCH` |
| `extclient` | The same on `/ext`: direct and indirect blocks, the sparse file, the four kinds of links, the loop, and the write check in `scratch` |
| `cfile` | The C library only (`open`, `read`, `write`, `lseek`, `close`, `unlink`, `mkdir`, `rmdir`, `rename`, `errno`) on `/ext` and `/fat` |
| `mute`, `muteclient` | `mute` reads one message and exits without replying; `muteclient` checks that a request through `vfs` to it fails instead of hanging, that the file system still answers and that a direct client fails too |
| `phoenix`, `phoenixclient` | `phoenix` (manifest `restart=5`) answers one `SYS_PING`, through the reply token, and exits; `phoenixclient` finds it again through the name server after each exit, three times, and prints `phoenixclient: ok` when every round is answered by a new process |
| `sigprobe` | Signals through `pm` (`ipc=pm,console`): a forked child catches terminate, another ignores terminate and interrupt and is ended by kill (status 137), refusals for an ended process, an unknown signal, a child signalling its parent, stop and continue (the stopped child shows `STOPPED`, does not run and finishes after continue, and can be killed), a direct `SYS_PROC_SIGNAL` and a forged `SIGNAL` message; the end to end test requires `sigprobe: ok` |
| `forkprobe` | Forks through `pm` (`ipc=pm,console`): each side changes a global and a stack variable and keeps its own value, the child forks again through its inherited handle, the child and the grandchild connect to `console` and are refused `vfs` by the name server (the rights of the parent), and the parent and the child collect their children with `wait` and read the exit statuses; the end to end test requires `forkprobe: ok` |
| `pmclient` | Lists the table of `pm` and checks the first rows, its own row (child of `rs`) and a row of an exited process, and that a caller that is not `rs` or `init` is refused (`ALLOCATE`, `REGISTER`); the end to end test requires `pmclient: ok` |
| `crasher` | Exits at once with status 7 (manifest `restart=3`): `rs` restarts it three times and prints `init: crasher gave up after 3 restarts` |
| `spawner` | Holds the `SPAWN` capability (manifest `caps=spawn`) and no grant authority: waits for its grant, checks that `SYS_PROC_CREATE` with a label it cannot give is refused and that a loader that makes mistakes (bad ranges, overlaps, writes into unmapped or sealed pages, an entry outside the image, too much memory, too many pending processes, a stale handle) is refused by the kernel, then starts a copy of `hello` and prints `spawner: ok` |
| `capgiver`, `capreceiver` | Capabilities in messages: the giver (manifest `caps=io:0x2F8+8*,irq:10*`) sends two copies of port ranges to the receiver, which accepts only `IO_PORTS`; the giver reaches the receiver through a handle from the name server and is refused for a type that was not accepted, a range it does not hold, an empty range, an unknown type, itself and a send by process id; the receiver answers through a reply token, opens the ports, cannot stretch the copies or pass on the one without the delegate flag, passes on the other, and loses only the first copy when the giver revokes it. Prints `capgiver: ok` and `capreceiver: ok` |
| `epgiver`, `epserver` | Endpoints and handles: the giver (manifest `caps=io:0x2E8+8*`, which gives it grant authority) creates an endpoint, binds it to the server and gives itself two handles with badges (one for the traffic, one for the stages). The server sees the badge as the peer of a call and in a message, and sees no forged badge; with no handle of its own (but the one on `names` it was born with) it cannot send or call with any of 256 guessed numbers, create an endpoint or bind one, and the giver cannot reach it by its process id. The giver sends a request that asks for a reply token: the server forwards it to itself through a second handle, answers it, and finds the token spent; the giver gets the answer, and checks that 1024 token values nobody gave fail and that the seventeenth open token is refused. The giver also checks that a revoked handle fails and that binding the endpoint to another process moves it. Prints `epgiver: ok` and `epserver: ok` |
| `modeserver`, `modeprobe` | Owner and mode in the name server: `modeserver` only waits for messages and runs twice (`owned`, `owner=modeprobe mode=x-`; `locked`, `owner=owned mode=x-`); `modeprobe` lists both in `ipc=` and checks that it, the owner, connects to `owned`, that it is refused (`DENIED`) by `locked`, which it does not own, and that it still connects to `console`; prints `modeprobe: ok` |
| `fuzz` | Syscall fuzzer: random and hostile arguments, three multi-thread scenarios, structured phases for the file servers and the loader, a final sanity check; see `documentation/userland/fuzzer.md` |

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
- In root mode the random `SYS_PROC_*` calls only receive random handles and ranges; the paths that need a real pending process are covered by the `spawner` test and by the corrupt images of the structured phase of the fuzzer.
- `muteclient` and the fuzzer test a server dying; `phoenix` and `crasher` test `rs` restarting a program and its limit. No real server (`ramfs`, `ata`) is restarted in a test.
- Nothing here runs as a kernel suite: those are in `test/cock/` and run with `scripts/qemu_test.sh`.

## Documentation

- [Userland programs](../../documentation/userland/system/programs.md) and [testing](../../documentation/userland/testing/README.md)
- [File system tests](../../documentation/userland/servers/fs/filesystem.md)
- [Syscall fuzzer](../../documentation/userland/testing/fuzzer.md)
- [Block storage tests](../../documentation/userland/drivers/block_storage.md)
- [Console tests](../../documentation/userland/drivers/console.md)
