# init

The first process of the system as Linux sees it: process id 1 in the table of `pm`. It has no authority at all. Its job today is to start the programs of the manifest that carry `parent=init` (the shell) and to start them again when they end. Adopting orphans comes with `wait` (stage L of the roadmap).

It is not the program the kernel starts first; that is `rs`, which holds the root capabilities. `rs` starts `init` through `pm`, after the servers it needs, and `init` asks `pm` for the rest.

## Where it sits

```text
rs   --SPAWN (flag AS_INIT)--> pm --> init
init --connect "pm", "vfs"--> names
init --read /etc/manifest.conf, /drivers, /servers or /bin/<program>--> vfs
init --ALLOCATE, SPAWN--> pm --> shell
pm   --PM_SPAWNED--> rs --BIND, equip--> names
```

- The manifest line is `init path=servers/init restart=3 ipc=pm,vfs,console`. `rs` restarts `init` if it dies, and `pm` gives the new one id 1 again.
- `init` reads `/etc/manifest.conf` through `vfs`, parses it with the shared parser and starts, in file order, every entry with `parent=init`. The label of an entry is `abi::service::programLabel(index)`, the same `rs` computes.
- `pm` serves `init` only for labels that `rs` permitted, with no IPC labels and the initial handle on the name server; `init` cannot name anything else. A compromised `init` can start the programs the manifest gives it and nothing it could use to pass for a server.
- `rs` finishes the start of each process `init` asks for (registers it with the name server and equips it); until then the name server holds the first `connect` of the new program for up to two seconds.

## Keeping the programs

Every 250 ms `init` looks at each program with `process_alive` and starts it again if it ended, up to eight times after the first start; then it prints `init: <name> gave up after 8 restarts`. Polling stands in for `wait`, which `pm` does not offer yet.

## Limits and known gaps

- It reads the manifest again instead of being told what to start; the manifest is the single list.
- It is not a shell and runs no script. When `wait` exists it will collect the zombies `pm` adopts to it.
- Only entries marked `parent=init` can be started by it, one copy each and without `restart=`.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: connects to `pm` and `vfs`, loads the manifest, starts the programs |
| `src/manifest.cpp`, `include/init/manifest.hpp` | `loadManifest`: reads and parses `/etc/manifest.conf` |
| `src/launch.cpp`, `include/init/launch.hpp` | `launch`: reads the image into an object `pm` made and asks for the process |
| `src/keep.cpp`, `include/init/keep.hpp` | `keepRunning`: starts the programs and starts them again |
| `Makefile` | `NAME = init`, a minimal `LIBC_SRCS`; the console client stays on |

## Documentation

- [pm](../../pm/README.md)
- [rs](../../rs/README.md)
- [pm and init](../../../documentation/userland/pm.md)
