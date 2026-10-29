# The manifest

`etc/manifest.conf` in the initrd (`/etc/manifest.conf` through `vfs`) says which programs run and what authority each one gets. It is plain text, one entry per line; empty lines and lines that start with `#` are ignored. The parser is `shared/include/cock/abi/manifest.hpp`, which the host tests cover.

```
vga_driver caps=mem:0xA0000+0x20000
fsclient path=bin/fsclient repeat=10
kbd path=drivers/kbd caps=io:0x60+1,irq:1 ipc=tty
```

| Part | Meaning |
|------|---------|
| first word | The name of the entry. It is the process name; names are unique and at most 31 bytes |
| `path=` | A file of the initrd that `rs` reads and spawns. Without it the entry is a boot module that already runs, and `rs` only grants it what the line says |
| `caps=` | Comma separated grants: `io:first+count` (ports), `irq:line` and `mem:first+size` (physical memory) and `spawn` (the right to call `SYS_PROC_CREATE`, with no range). A final `*` (`io:0x2F8+8*`) lets the program pass the capability on, to another program or in a `CAPABILITY_TRANSFER` message; without it the grant cannot be passed on. Numbers are decimal or `0x` hexadecimal. The count of `io` and `irq` defaults to 1, `mem` needs it. At most 4 grants |
| `arg=` | A number (decimal or `0x`) that travels in the first argument of the `CAPABILITY_GRANTED` message, for a server that needs one value (the device of `fat`). It makes `rs` send the message even when the line has no grants |
| `ipc=` | Comma separated names of the programs this one may talk to: send messages, make synchronous calls and share memory. Up to 8 names, no repeats. It is the policy the name server checks when the program connects to one of them (`names::connect`): the caller gets an `IPC` capability for the service label of the server then, not at start. The policy is symmetric: it allows the connection when either program lists the other. The label of an entry is `PROGRAM_SERVICE_BASE` plus its position in the manifest (and `ramfs`, which has no entry, has `RAMFS_SERVICE`), so it is the same after a restart. The right works in both directions: when `sh` may talk to `console`, `console` may answer `sh` |
| `repeat=` | Run the program that many times (1 to 32), one after the other, waiting for each to exit. Only for entries with `path=` |
| `restart=` | After the manifest is done, `rs` starts the program again each time it ends, at most that many times (1 to 16) in a window of 60 seconds of its own waiting time; past the limit it gives up. Only for entries with `path=` and no `repeat=`. See `userland/servers/rs/README.md` |
| `parent=` | Only `init`: `init`, not `rs`, starts the program, through `pm`. Needs `path=`, one copy and no `restart=`. See [pm](../servers/pm.md) |
| `owner=` | The name of the program that owns this one. With `mode=` it decides who may connect to this program through the name server, besides the `ipc=` lists. See [names](../servers/names.md) |
| `mode=` | Two characters, `x` to allow and `-` to refuse: the first for the owner, the second for every other program (`x-` only the owner, `-x` everybody but the owner, `xx` the default). A refused `connect` answers `DENIED`. See [names](../servers/names.md) |
| `notify=` | Only `fork`: the program keeps state for each process (`vfs` its open files, `pipe` the ends of pipes), so `rs` sends it a `PM_FORKED` message for every copy made by `fork`, and it gives the copy what its parent had. Needs `path=`. See [pm](../servers/pm.md) |

When an entry has grants or an argument, `rs` sends the process `CAPABILITY_GRANTED` after granting. The kernel still checks every grant against what `rs` holds, so a line cannot give more than `rs` has. The parser rejects the whole file at the first bad line (unknown key, repeated key, bad number, range that wraps the address space, too many entries or grants, repeated name) and `rs` then starts nothing and exits; the serial log shows the line number and the error. The file is at most 4096 bytes and 32 entries. Adding a driver means adding a line here, not editing `rs`.
