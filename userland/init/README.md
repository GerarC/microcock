# init

The root server of MicroCOCK. It is the only process the kernel starts with authority: it holds the root capabilities and the right to grant them. Everything else in Ring 3 is started by `init`, according to a text file, and receives only what that file says.

## Where it sits

- It is a Multiboot module named `init` (see `USERLAND_BOOT_ORDER` in the root `Makefile`: `init`, `names`, `ramfs`, `vga_driver`). The kernel gives the root capabilities to the first program module it loads, which the boot order makes `init`; it also gives it the service label `ROOT_SERVICE`.
- It needs no line in the manifest: it is the one that reads it. Its own policy is written in the code: bring the name server up, and grant `ramfs` the `initrd` boot module and tell it so.
- It starts the system and exits with status 0, unless an entry has `restart=`: then it stays alive and starts those programs again when they end (see "Restarting servers"). It exits with status 1 if the name server (`names`), `ramfs` or the `initrd` module is missing, if `manifest.conf` cannot be read, or if the parser rejects a line.
- The C library console client is switched off (`CONSOLE_CLIENT = no`): `init` prints before `console` has its capabilities.

## Protocol

| Direction | Message | Use |
|-----------|---------|-----|
| `init` to `names` | `CAPABILITY_GRANTED` (after the label and a `SYS_CAP_GRANT` of `ENDPOINT` with `GRANT_DELEGATE` and `GRANT_MINT`) | Lets the name server serve |
| `init` to `names` | Synchronous calls `DEFINE`, `ALLOW` and `BIND`, through a handle `init` made for itself | Describe the programs, the `ipc=` policy and the process that runs each program (see [names](../../documentation/userland/names.md)) |
| `init` to `ramfs` | `CAPABILITY_GRANTED` (after `SYS_CAP_GRANT` of `BOOT_MODULE`) | Lets `ramfs` open the `initrd` |
| `init` to `ramfs` | `FS_CONNECT`, `FS_OPEN`, `FS_STAT`, `FS_READ`, `FS_CLOSE` | Reads `manifest.conf` and each program image, through `cock::user::fs::Client` |
| `init` to each started process | `CAPABILITY_GRANTED`, `arg0` is the `arg=` value | Sent after the grants, only when the entry has grants or an argument, through a handle on the endpoint of the program that lives only for the send (`sendTo`) |

Every program `init` spawns also starts with an **initial handle** on the endpoint of the name server (`SpawnRights::initialEndpoint`, built in `rightsOf`), badged with its process id, so it can `connect` from its first instruction; no message is involved.
| kernel to `init` | `PROCESS_EXITED` | Only for `repeat=` and `restart=` entries, which `init` spawns with the exit notice requested and supervises |

The manifest grammar (`shared/include/cock/abi/manifest.hpp`) has these keys after the entry name:

| Key | Meaning |
|-----|---------|
| `path=` | File of the initrd to spawn; without it the entry is a boot module that is only configured |
| `caps=` | Up to four grants: `io:first+count`, `irq:line`, `mem:first+size`, `spawn` (the right to call `SYS_PROC_CREATE`) |
| `arg=` | One number that travels in `CAPABILITY_GRANTED` (the block device of `fat` and `ext`) |
| `ipc=` | Names of the programs this one may connect to (send, call, share memory), up to eight; `init` sends them to the name server, which checks them when the program connects and gives a handle on the server then |
| `repeat=` | Run the program that many times in a row, 1 to 32 |
| `restart=` | Start the program again when it ends, at most that many times in one window, 1 to 16; needs `path=` and no `repeat=` |

## Restarting servers

An entry with `restart=N` is spawned with the exit notice requested and put in a table of watched programs. After the last entry has run, `init` waits for exit notices. When a watched program ends it prints `init: <name> ended with status <n>`, spawns it again from the same entry, gives it the same capabilities and argument, tells the name server which process runs it now, and prints `init: restarted <name> <k> of <N>`. The new process has a new process id and the same name, so a client finds it by connecting through the name server again, which gives no new capability; a call that was blocked on the old one has already failed as it does for any dead server. After `N` restarts in one window (`RESTART_WINDOW_MILLISECONDS`, 60000) `init` prints `init: <name> gave up after <N> restarts` and stops watching it; the counter starts again when the window is over. `init` has no clock, so the window counts only the time it waits with nothing to do (a program that crashes in a tight loop reaches its limit sooner, never later). When no program is watched any more, `init` exits.

The file client of `init` keeps the exit notices that arrive while it waits for a file reply (`keepExitNotice`, in the stash of the C++ API); without that hook they are dropped and a restart never happens.

## What the manifests grant

| Entry | Source | Grants and arguments |
|-------|--------|----------------------|
| `vga_driver` | `initrd/manifest.conf` | `mem:0xA0000+0x20000`, boot module |
| `console` | `initrd/manifest.conf` | `io:0x3F8+8` |
| `ata` | `initrd/manifest.conf` | `io:0x1F0+8`, `io:0x3F6+1` |
| `rtc` | `initrd/manifest.conf` | `io:0x70+2` |
| `fat`, `ext` | `initrd/manifest.conf` | `arg=2`, `arg=3`, no capabilities |
| `vfs`, `sh` | `initrd/manifest.conf` | none |
| `kbd` | `initrd/manifest.conf` | `io:0x60+1`, `irq:1` |

All of these except `vga_driver` and `sh` also have `restart=3`.
| `fsclient`, `probe`, `ext4probe` and the other test programs | `test/initrd/manifest.conf` | none (`ext4probe` is `ext.elf` with `arg=4`); only with `TEST_INITRD=1` |

The kernel still checks every grant against what `init` holds, so a manifest line cannot give more than `init` has.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: `startNames`, `grantInitrd`, `loadManifest`, `describeSystem`, `runEntry` for each entry, exit |
| `include/init/directory.hpp`, `src/directory.cpp` | `startNames()`, `describeSystem()`, `bindService()`: what `init` tells the name server |
| `include/init/endpoints.hpp`, `src/endpoints.cpp` | `endpointOf()`, `bindEndpoint()`, `handleOf()` and `sendTo()`: the endpoint of each label and the handles `init` makes for itself |
| `include/init/boot.hpp`, `src/boot.cpp` | `grantInitrd()`: the policy in the code, the label of `ramfs` and the `initrd` boot module for it; returns its process id |
| `include/init/service.hpp`, `src/service.cpp` | `labelOf()`, `labelOfName()`, `rightsOf()` and `labelBootModule()`: the service label of each entry |
| `include/init/grant.hpp`, `src/grant.cpp` | `announce()` and `equip()`: grants of an entry and the `CAPABILITY_GRANTED` message |
| `include/init/files.hpp`, `src/files.cpp` | `readInto()`: a whole file through the file client |
| `include/init/manifest.hpp`, `src/manifest.cpp` | `loadManifest()`: connect, read `manifest.conf`, parse it; holds the parsed `manifest` |
| `include/init/spawn.hpp`, `src/spawn.cpp` | `spawnProgram()`: reads the image into an anonymous mapping and calls `cock::user::spawn` |
| `include/init/supervise.hpp`, `src/supervise.cpp` | `awaitExit()`: polls `PROCESS_EXITED` and `process_alive`; `keepExitNotice()`: the hook of the file client |
| `include/init/watch.hpp`, `src/watch.cpp` | `watchProgram()`, `anyWatched()`, `superviseServers()`: the table of watched programs and the restart loop |
| `include/init/run.hpp`, `src/run.cpp` | `runEntry()`: configure a boot module or spawn `repeat` times; registers `restart=` programs |
| `Makefile` | `NAME = init`, `SRCS`, a minimal `LIBC_SRCS` list, includes `../common.mk` |

## Build, run, test

```sh
make userland/init
make iso
make run-headless
bash scripts/qemu_userland_test.sh
```

On a machine without the cross compiler add `HOST_TOOLCHAIN=1`. The end to end script boots with the test initrd and requires these lines on the serial log: `init: initrd is boot module`, `names: ready`, `init: manifest has 29 entries`, `init: configured vga_driver`, `init: started <name>` for each program, `init: fsclient exit notices 10/10`, `init: restarted phoenix 2 of 5` and `init: crasher gave up after 3 restarts`. The manifest parser has host tests in `test/host/test_manifest.cpp` (`bash scripts/host_test.sh`).

## Limits and known gaps

- A program image is at most 1 MiB and must fit the quota of `init`; the manifest is at most 4096 bytes and 32 entries.
- The capability policy is a text file read from the initrd: the right to talk to a program is given by the name server when a program connects, and there is no DMA policy yet (stage 9 of the roadmap). The kernel can revoke a grant (`SYS_CAP_REVOKE`), but `init` does not use it.
- Entries run in file order and a `repeat=` entry blocks the rest until its runs end.
- `console`, `ata`, `rtc`, `fat`, `vfs`, `kbd` and `ext` have `restart=3` in `initrd/manifest.conf`. `ramfs` has none (`init` itself reads files through it and gives it the `initrd`), `vga_driver` is a boot module, and `sh` is not a server. Restarting `vga_driver` or `ramfs` is not done.
- Each holder of a server's process id finds the new one by name: `vfs` reconnects a mount the next time it is used, the C library and the shell reconnect to `vfs` after a `NOT_CONNECTED` failure (their open files are forgotten), `kbd` connects to `console` again when a send fails, the console client of the C library connects again when the known server is gone, and the block and clock clients used by `fat` and `ext` connect again on the next call after the server was found gone (`attached()` in `block.hpp`, `lost` in `clock.hpp`). A client reconnect does not release the old shared window.
- None of these reconnections has been exercised by killing a real server: the restart loop is tested with `phoenix` and `crasher`, and the boot with the new `restart=` lines passes.
- A restarted program gets its grants again, but anything it kept in memory (open sessions, mounts) is lost.
- `init` supervises only programs it started with `restart=`; with none, it exits when the manifest is done.
- At most 16 programs can be watched or have a pending exit notice at once (`MAX_NOTIFIED_CHILDREN`).

## Documentation

- [Userland programs, manifest and API](../../documentation/userland/userland.md)
- [File system boot sequence](../../documentation/userland/filesystem.md)
- [Capabilities](../../documentation/core/capabilities.md)
- [Task management and spawn](../../documentation/core/task_management.md)
