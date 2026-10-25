# The process manager, `rs` and `init`

MicroCOCK keeps the process model out of the kernel, as Minix 3 does. Three programs share the work that the `init` of a Unix kernel does, and none of them is in Ring 0:

| Program | Role | Authority |
|---------|------|-----------|
| `rs` | Decides what every program may do, starts the servers and starts them again when they end | The root capabilities: ports, interrupt lines, physical memory, boot modules, the right to grant |
| `pm` | Creates the processes of the system after boot, is their parent in the kernel and keeps the table | The right to create processes and the program labels; no hardware |
| `init` | The first process as Linux sees it (id 1); starts the shell and starts it again | None |

The kernel only offers primitives (`SYS_PROC_CREATE` and its five companions). Stage L of the roadmap adds `wait`, `exec`, signals and `fork` as requests to `pm`, with small primitives in the kernel; this page describes the first step.

## Boot

1. GRUB loads the kernel and the modules `rs`, `names`, `ramfs`, `vga_driver` and the `initrd`. The first module, `rs`, gets the root capabilities.
2. `rs` brings the name server up, gives `ramfs` the `initrd`, reads `manifest.conf` through `ramfs`, describes the programs to the name server and configures the boot modules (`vga_driver`).
3. `rs` creates `pm` itself: nothing can create it for `rs`. It binds `pm` to its endpoint and to the name server, grants it the right to create processes (the manifest line `pm path=bin/pm.elf caps=spawn`), an `IPC` capability for the program labels, an `ENDPOINT` capability on the endpoint of the name server and a handle on its own endpoint, and sends `CAPABILITY_GRANTED`. It then records `rs`, `pm`, `names` and `ramfs` in the table of `pm` (`REGISTER`), and `vga_driver` when it configures it.
4. `rs` creates every other program of the manifest through `pm` (`console`, `ata`, `rtc`, `fat`, `vfs`, `kbd`, `ext`, then `init`), reading the image straight into an object that `pm` made. After each one it binds the process to the name server and equips it; the kernel parent is `pm`.
5. `init` connects to `pm` and `vfs`, reads `/manifest.conf` and asks `pm` for the programs marked `parent=init` (the shell). `pm` tells `rs` (`PM_SPAWNED`), which registers each one with the name server.
6. `rs` keeps serving the notices of `pm` for ever: when a program it watches ends (`PM_EXITED`) it creates it again.

## Why `pm` is not given more

The kernel checks the rights of a process at `SYS_PROC_START`: the creator must hold the right to create processes, a grantable `IPC` capability for the label of the child and for each label of its list, and a grantable `ENDPOINT` capability for the initial handle. `rs` gives `pm` those and nothing else, so no kernel change was needed and `pm` never holds a port or device memory. `rs` keeps granting the hardware after the process exists (`equip`).

The price is trust: `pm` can give a new process any program label, so a flaw in `pm` is a flaw in the policy of the name server. `pm` obeys only the root server for labels; `init` may use only the labels `rs` permitted, with no IPC labels and the initial handle chosen by `pm`.

## Protocol of `pm`

Synchronous calls of four words, `shared/include/cock/abi/pm.hpp`, through a handle whose badge is the process id of the caller.

| Operation | Served for | Meaning |
|-----------|------------|---------|
| `ALLOCATE bytes` | `rs`, `init` | `pm` makes a shared object of that size, shares it with the caller for writing and returns its id; one per caller |
| `SPAWN object` | `rs`, `init` | The first page holds a `SpawnRequest` (name, `SpawnRights`, flags, image size), the image starts at `IMAGE_OFFSET`; `pm` copies the request, checks it and creates the process |
| `REGISTER kernel parent wanted` | `rs` | Records a process that already runs |
| `PERMIT label` | `rs` | `init` may create processes with that label |
| `LIST index` | anyone with a handle | One row of the table, with the state in bits 8 to 15 of the reply header |

## Process states

The states are those of `ps` in Linux (`abi::pm::State`). The table stores only two of them, `RUNNING` for a process that lives and `EXITED` (`X`) for one whose exit notice arrived. `LIST` does not report the stored `RUNNING` as it is: it asks the kernel for the states of the live threads (`SYS_PROC_STATE`) and `cock::pm::classify` (`shared/include/cock/pm/classify.hpp`) turns the mask into `R` when a thread is ready or running and into `S` when every live thread waits (timer, futex, message, accept or reply). Nothing is reported by the scheduler and nothing is stored, so the thread path of the kernel has no extra work, and the answer is the state at the moment of the call. When the kernel cannot read the threads, for example because the first thread of the process was already reaped while another thread lives, `LIST` answers `R`.

`T` (stopped), `Z` (zombie) and `D` (uninterruptible sleep) have a value in `State` and a letter in `ps`, and nothing produces them: `Z` comes with `wait` (L1), `T` with signals (L3), and `D` with the first wait that a signal must not end, which sets it and says so in its header.

The size of the object is `pm`'s own, so a caller cannot make `pm` read past it. `pm` checks that the name ends, the flags are known (`FORWARD_EXIT`, `AS_INIT`), the image fits the object, the label is a program label and there are at most eight IPC labels. A process created for `init` gets no IPC label and an initial handle on the name server.

Two notices travel the other way, as ordinary messages through a handle that `rs` gave `pm`; the kernel stamps the badge, which is the process id of `pm`, and `rs` ignores any other:

| Message | Meaning |
|---------|---------|
| `PM_EXITED` | `arg0` kernel process id, `arg1` exit status: a process created with `FORWARD_EXIT` ended |
| `PM_SPAWNED` | `arg0` kernel process id, `arg1` label: a process that `init` asked for exists |

A notice that finds the inbox of `rs` full is tried again every 50 ms.

## The table

The id in the table is not the kernel one. It counts from 2 in the order processes arrive; id 1 is `init`, and a restarted `init` takes it again. The parent is the id in the table of whoever asked for the process: `rs` for the servers, `init` for the shell. A row has the kernel id, the id, the parent, the state (running or exited) and the exit status; the row of a process that exited stays until no row is free. The table is a header (`shared/include/cock/pm/table.hpp`) with host tests.

## Limits

- No `wait`, `exec`, signals or `fork` yet, and no zombie that anyone collects.
- If `pm` dies the table and the notices of its children are lost, because their parent disappears. `rs` could start another one but not rebuild the table. Minix has the same limit.
- The boot modules are in the table but are never marked as exited: the kernel sends no notice for them.
- A process may have at most `MAX_NOTIFIED_CHILDREN` (64) children with a pending exit notice, and `pm` has 128 rows.
- `init` polls `process_alive` every 250 ms to keep the shell; `wait` will replace that.

See also the READMEs of [`pm`](../../userland/pm/README.md), [`rs`](../../userland/rs/README.md) and [`init`](../../userland/init/README.md), [capabilities](../core/capabilities.md) and [the name server](names.md).
