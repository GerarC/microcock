# rs

The root server of MicroCOCK, the `rs` (reincarnation server) of Minix 3. It is the only process the kernel starts with authority: it holds the root capabilities and the right to grant them. It decides what every program of a text file may do, starts them through the process manager `pm`, and starts the servers again when they end. It does not keep the table of processes (that is `pm`) and it does not run the shell (that is `init`).

## Where it sits

- It is a Multiboot module named `rs` (see `USERLAND_BOOT_ORDER` in the root `Makefile`: `rs`, `names`, `ramfs`, `vga_driver`). The kernel gives the root capabilities to the first program module it loads, which the boot order makes `rs`; it also gives it the service label `ROOT_SERVICE`.
- It needs no line in the manifest: it is the one that reads it. Its own policy is written in the code: bring the name server up, and grant `ramfs` the `initrd` boot module and tell it so.
- It starts the system and then stays alive: it serves the notices of `pm` for ever (see "Restarting servers"). It exits with status 1 if the name server (`names`), `ramfs` or the `initrd` module is missing, if `manifest.conf` cannot be read, or if the parser rejects a line.
- The C library console client is switched off (`CONSOLE_CLIENT = no`): `rs` prints before `console` has its capabilities.

## Protocol

| Direction | Message | Use |
|-----------|---------|-----|
| `rs` to `names` | `CAPABILITY_GRANTED` (after the label and a `SYS_CAP_GRANT` of `ENDPOINT` with `GRANT_DELEGATE` and `GRANT_MINT`) | Lets the name server serve |
| `rs` to `names` | Synchronous calls `DEFINE`, `ALLOW`, `OWNER` and `BIND`, through a handle `rs` made for itself | Describe the programs, the `ipc=` policy, the owner and mode, and the process that runs each program (see [names](../../documentation/userland/names.md)) |
| `rs` to `ramfs` | `CAPABILITY_GRANTED` (after `SYS_CAP_GRANT` of `BOOT_MODULE`) | Lets `ramfs` open the `initrd` |
| `rs` to `ramfs` | `FS_CONNECT`, `FS_OPEN`, `FS_STAT`, `FS_READ`, `FS_CLOSE` | Reads `manifest.conf` and each program image, through `cock::user::fs::Client` |
| `rs` to each started process | `CAPABILITY_GRANTED`, `arg0` is the `arg=` value | Sent after the grants, only when the entry has grants or an argument, through a handle on the endpoint of the program that lives only for the send (`sendTo`) |
| `rs` to `pm` | Synchronous calls `ALLOCATE`, `SPAWN`, `REGISTER`, `PERMIT` | `rs` creates `pm` itself (`spawnDirect`) and every other program through it; see [pm](../pm/README.md) |
| `rs` to `pm` | `CAPABILITY_GRANTED`, `arg0` a handle on the endpoint of `rs`, `arg1` the endpoint of the name server | Sent after `pm` got the right to create processes, the program labels and the endpoint of the name server |
| `pm` to `rs` | `PM_EXITED` (`arg0` kernel process id, `arg1` status) | For the entries with `repeat=` and `restart=`, which `rs` spawns with the notice requested; the badge is the process id of `pm` |
| `pm` to `rs` | `PM_SPAWNED` (`arg0` kernel process id, `arg1` label) | A process that `init` asked for exists: `rs` registers it with the name server and equips it |
| `pm` to `rs` | `PM_FORKED` (`arg0` kernel process id of the copy, `arg1` that of its parent) | A process made a copy of itself: `rs` tells the name server (`Op::FORKED`), which gives the copy the rights of its parent and nothing else. `rs` does not look at the policy; the name server knows the parent |
| `rs` to the programs with `notify=fork` | `PM_FORKED`, the same message (`arg0` the copy, `arg1` its parent) | Sent through the handle `rs` makes for the program, so the badge is the process id of `rs`. The program copies the state of the parent for the copy. Sent before the name server is told |
| `rs` to `pm` | `PM_FORK_SETTLED` (`arg0` kernel process id of the copy) | Sent last, so `pm` answers the parent of the copy: what the parent does next cannot change what the copy inherited |

Every program `rs` spawns also starts with an **initial handle** on the endpoint of the name server (`SpawnRights::initialEndpoint`, built in `rightsOf`), badged with its process id, so it can `connect` from its first instruction; no message is involved.

The manifest grammar (`shared/include/cock/abi/manifest.hpp`) has these keys after the entry name:

| Key | Meaning |
|-----|---------|
| `path=` | File of the initrd to spawn; without it the entry is a boot module that is only configured |
| `caps=` | Up to four grants: `io:first+count`, `irq:line`, `mem:first+size`, `spawn` (the right to call `SYS_PROC_CREATE`) |
| `arg=` | One number that travels in `CAPABILITY_GRANTED` (the block device of `fat` and `ext`) |
| `ipc=` | Names of the programs this one may connect to (send, call, share memory), up to eight; `rs` sends them to the name server, which checks them when the program connects and gives a handle on the server then |
| `owner=` | Name of the program that owns this one; `rs` sends the name server an `OWNER` for it |
| `mode=` | Who may connect to this program: two characters, `x` or `-`, for its owner and for every other program (`xx` is the default); the name server refuses a refused class with `DENIED` |
| `repeat=` | Run the program that many times in a row, 1 to 32 |
| `restart=` | Start the program again when it ends, at most that many times in one window, 1 to 16; needs `path=` and no `repeat=` |
| `parent=` | `init` is the only value: `init`, not `rs`, starts the program, through `pm`; needs `path=`, one copy and no `restart=`. `rs` still defines it in the name server and tells `pm` that `init` may create it |
| `notify=` | `fork` is the only value: `rs` sends the program a `PM_FORKED` message for every copy made by `fork`, so it can give the copy the state of the parent; needs `path=` |

## Restarting servers

An entry with `restart=N` is spawned (through `pm`) with the exit notice requested and put in a table of watched programs. After the last entry has run, `rs` waits for the notices `pm` forwards (`PM_EXITED`); it does that for ever. When a watched program ends it prints `rs: <name> ended with status <n>`, spawns it again from the same entry, gives it the same capabilities and argument, tells the name server which process runs it now, and prints `rs: restarted <name> <k> of <N>`. The new process has a new process id and the same name, so a client finds it by connecting through the name server again, which gives no new capability; a call that was blocked on the old one has already failed as it does for any dead server. After `N` restarts in one window (`RESTART_WINDOW_MILLISECONDS`, 60000) `rs` prints `rs: <name> gave up after <N> restarts` and stops watching it; the counter starts again when the window is over. `rs` has no clock, so the window counts only the time it waits with nothing to do (a program that crashes in a tight loop reaches its limit sooner, never later). When no program is watched any more, `rs` exits.

The file client of `rs` takes in the notices that arrive while it waits for a file reply (`keepExitNotice`): a `PM_EXITED` goes to the stash of the C++ API and a `PM_SPAWNED` is served at once (`startedForInit`) and so is a `PM_FORKED` (`registerFork`); without that hook they are dropped and a restart never happens.

## What the manifests grant

| Entry | Source | Grants and arguments |
|-------|--------|----------------------|
| `vga_driver` | `initrd/manifest.conf` | `mem:0xA0000+0x20000`, boot module |
| `console` | `initrd/manifest.conf` | `io:0x3F8+8` |
| `ata` | `initrd/manifest.conf` | `io:0x1F0+8`, `io:0x3F6+1` |
| `rtc` | `initrd/manifest.conf` | `io:0x70+2` |
| `fat`, `ext` | `initrd/manifest.conf` | `arg=2`, `arg=3`, no capabilities |
| `vfs` | `initrd/manifest.conf` | none |
| `pm` | `initrd/manifest.conf` | `spawn`; `rs` adds in the code an `IPC` capability for the program labels and an `ENDPOINT` capability on the name server |
| `init` | `initrd/manifest.conf` | none, `restart=3` |
| `sh` | `initrd/manifest.conf` | none, `parent=init` |
| `kbd` | `initrd/manifest.conf` | `io:0x60+1`, `irq:1` |

All of these except `pm`, `vga_driver`, `init` (it has its own `restart=3`) and `sh` also have `restart=3`.
| `fsclient`, `probe`, `ext4probe` and the other test programs | `test/initrd/manifest.conf` | none (`ext4probe` is `ext.elf` with `arg=4`); only with `TEST_INITRD=1` |

The kernel still checks every grant against what `rs` holds, so a manifest line cannot give more than `rs` has.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: `startNames`, `grantInitrd`, `loadManifest`, `describeSystem`, `runEntry` for each entry, exit |
| `include/rs/directory.hpp`, `src/directory.cpp` | `startNames()`, `describeSystem()`, `bindService()`: what `rs` tells the name server |
| `include/rs/forks.hpp`, `src/forks.cpp` | `forwardFork()` and `acknowledgeFork()`: `PM_FORKED` to the programs with `notify=fork`, and `PM_FORK_SETTLED` to `pm` |
| `include/rs/endpoints.hpp`, `src/endpoints.cpp` | `endpointOf()`, `bindEndpoint()`, `handleOf()` and `sendTo()`: the endpoint of each label and the handles `rs` makes for itself |
| `include/rs/boot.hpp`, `src/boot.cpp` | `grantInitrd()`: the policy in the code, the label of `ramfs` and the `initrd` boot module for it; returns its process id |
| `include/rs/service.hpp`, `src/service.cpp` | `labelOf()`, `labelOfName()`, `rightsOf()` and `labelBootModule()`: the service label of each entry |
| `include/rs/grant.hpp`, `src/grant.cpp` | `announce()` and `equip()`: grants of an entry and the `CAPABILITY_GRANTED` message |
| `include/rs/files.hpp`, `src/files.cpp` | `readInto()`: a whole file through the file client |
| `include/rs/manifest.hpp`, `src/manifest.cpp` | `loadManifest()`: connect, read `manifest.conf`, parse it; holds the parsed `manifest` |
| `include/rs/spawn.hpp`, `src/spawn.cpp` | `spawnProgram()`: reads the image into an object `pm` made and asks `pm` to create the process; `spawnDirect()`: the way `pm` itself is created |
| `include/rs/pm.hpp`, `src/pm.cpp` | `startPm()`, `pmClient()`, `registerProcess()`, `isExitNotice()`, `isSpawnNotice()`, `startedForInit()`, `flagsOf()`: start `pm`, give it what it needs and take in its notices |
| `include/rs/supervise.hpp`, `src/supervise.cpp` | `awaitExit()`: waits for `PM_EXITED` and polls `process_alive`; `keepExitNotice()`: the hook of the file client |
| `include/rs/watch.hpp`, `src/watch.cpp` | `watchProgram()`, `superviseServers()`: the table of watched programs and the loop that serves the notices of `pm` |
| `include/rs/run.hpp`, `src/run.cpp` | `runEntry()`: configure a boot module or spawn `repeat` times; registers `restart=` programs |
| `Makefile` | `NAME = rs`, `SRCS`, a minimal `LIBC_SRCS` list, includes `../common.mk` |

## Build, run, test

```sh
make userland/rs
make iso
make run-headless
bash scripts/qemu_userland_test.sh
```

On a machine without the cross compiler add `HOST_TOOLCHAIN=1`. The end to end script boots with the test initrd and requires these lines on the serial log: `rs: initrd is boot module`, `names: ready`, `rs: manifest has 42 entries`, `rs: started pm`, `pm: ready`, `rs: configured vga_driver`, `rs: started <name>` for each program, `rs: started init`, `init: ready`, `rs: started sh for init`, `init: started sh`, `rs: fsclient exit notices 10/10`, `pmclient: ok`, `rs: restarted phoenix 2 of 5` and `rs: crasher gave up after 3 restarts`. The manifest parser has host tests in `test/host/test_manifest.cpp` (`bash scripts/host_test.sh`).

## Limits and known gaps

- A program image is at most `abi::pm::MAX_IMAGE_BYTES` (2 MiB less a page) and must fit the quota of `pm`; the manifest is at most 4096 bytes and 48 entries.
- The capability policy is a text file read from the initrd: the right to talk to a program is given by the name server when a program connects, and there is no DMA policy yet (stage 9 of the roadmap). The kernel can revoke a grant (`SYS_CAP_REVOKE`), but `rs` does not use it.
- Entries run in file order and a `repeat=` entry blocks the rest until its runs end.
- `console`, `ata`, `rtc`, `fat`, `vfs`, `kbd` and `ext` have `restart=3` in `initrd/manifest.conf`. `ramfs` has none (`rs` itself reads files through it and gives it the `initrd`), `vga_driver` is a boot module, and `sh` is not a server. Restarting `vga_driver` or `ramfs` is not done.
- Each holder of a server's process id finds the new one by name: `vfs` reconnects a mount the next time it is used, the C library and the shell reconnect to `vfs` after a `NOT_CONNECTED` failure (their open files are forgotten), `kbd` connects to `console` again when a send fails, the console client of the C library connects again when the known server is gone, and the block and clock clients used by `fat` and `ext` connect again on the next call after the server was found gone (`attached()` in `block.hpp`, `lost` in `clock.hpp`). A client reconnect does not release the old shared window.
- None of these reconnections has been exercised by killing a real server: the restart loop is tested with `phoenix` and `crasher`, and the boot with the new `restart=` lines passes.
- A restarted program gets its grants again, but anything it kept in memory (open sessions, mounts) is lost.
- `rs` supervises only the programs it started with `restart=`; it never exits once the manifest is done, because it also serves `PM_SPAWNED`.
- At most 64 children of `pm` can have a pending exit notice at once (`MAX_NOTIFIED_CHILDREN`), and `pm` has 128 rows. A `PM_EXITED` that finds the inbox of `rs` full is tried again by `pm` every 50 ms.

## Documentation

- [Userland programs, manifest and API](../../documentation/userland/userland.md)
- [File system boot sequence](../../documentation/userland/filesystem.md)
- [Capabilities](../../documentation/core/capabilities.md)
- [Task management and spawn](../../documentation/core/task_management.md)
