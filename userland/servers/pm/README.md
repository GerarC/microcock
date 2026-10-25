# pm

The process manager. It creates the processes of the system after boot, is their parent in the kernel, and keeps the table of them: process id, parent, state. It is the first of the Linux-like process model (see the roadmap, stage L): `wait`, `exec` and `fork` are requests to it, signals will be, and the kernel only gains small primitives.

## Where it sits

```text
rs   --ALLOCATE, SPAWN, REGISTER, PERMIT--> pm --SYS_PROC_CREATE .. START--> kernel
init --ALLOCATE, SPAWN-------------------> pm
any  --LIST------------------------------> pm
kernel --PROCESS_EXITED--> pm --PM_EXITED, PM_SPAWNED, PM_FORKED--> rs
```

- It is not a boot module. `rs` reads `servers/pm` from the initrd and creates it itself (`spawnDirect`), because nothing can create it for `rs`. Every other program of the manifest is created by `pm` on request of `rs` or of `init`.
- It holds the right to create processes (`caps=spawn` in the manifest), an `IPC` capability over the labels of the programs (`PROGRAM_SERVICE_BASE` on, `PROGRAM_LABEL_COUNT` of them, grantable), an `ENDPOINT` capability on the endpoint of the name server (so the processes it creates can be given their initial handle) and a handle on the endpoint of `rs`. **No port, interrupt line or device memory**: `rs` keeps granting those after the process exists.
- A program is its kernel child, so the kernel sends `pm` the exit notice of each one (`MAX_NOTIFIED_CHILDREN`, 64). `pm` marks the row and, for the processes `rs` asked about, sends `rs` a `PM_EXITED` message with the same numbers.
- The kernel does not know `pm`. If it dies, the table and the notices of its children are lost, because their parent disappears; `rs` could start another one but not rebuild the table. Minix has the same limit.

## Protocol

Synchronous calls of four words through a handle on the endpoint of `pm`; the badge of the handle is the process id of the caller. The words are in `shared/include/cock/abi/pm.hpp`.

| Operation | Who | What |
|-----------|-----|------|
| `ALLOCATE bytes` | any process in the table | `pm` makes a shared object of that size, shares it with the caller for writing and returns its id. One object per caller; a slot of a caller that died is taken back when no other is free |
| `EXEC object` | any process in the table | Replaces the image of the caller with the one in the object (an `ExecRequest` and the image); on success the call never returns. See the documentation page |
| `FORK` | any process in the table | Makes a copy of the caller, with its memory shared copy on write and its `IPC` and endpoint capabilities; the reply to the caller carries the id in the table of the child, and the child, which resumes inside the same call, gets `NO_PID` |
| `KILL pid signal` | any process in the table | Sends interrupt, terminate (a `SIGNAL` message the program may ignore), kill (ends the process, status 137), stop or continue (no thread of the process runs until continue; `LIST` shows `T`) through `SYS_PROC_SIGNAL`; `rs` may signal any live process, anyone else only its descendants |
| `SPAWN object` | `rs`, `init` | The first page of the object holds a `SpawnRequest` (name, rights, flags, image size) and the image starts at `IMAGE_OFFSET`. `pm` copies the request, checks it and creates the process; the reply carries the kernel process id and the id in the table |
| `REGISTER kernel parent wanted` | `rs` | Adds a process that already runs (the boot modules, `pm` itself) |
| `PERMIT label` | `rs` | `init` may create processes with that label |
| `LIST index` | anyone with a handle | One row: kernel id, id in the table, parent, state |
| `WAIT pid` | anyone with a handle | Collects the status of a child (`ANY_CHILD` for any); blocks, with the call detached, until one exits; `NOT_FOUND` when there is none |

The size of the object is `pm`'s own, so a caller cannot make `pm` read past it by lying about the image size. `pm` checks that the name ends, the flags are known, the image fits the object, the label is a program label and there are at most sixteen IPC labels; the kernel checks the rights again against what `pm` holds.

`init` gets less than `rs`: only labels that `rs` permitted (the manifest entries with `parent=init`), no IPC labels, no flags, and the initial handle is always the one on the name server. After it creates a process, `pm` sends `rs` a `PM_SPAWNED` message so `rs` can register the process with the name server and equip it.

## Process ids

The id in the table is not the kernel one. The table numbers processes from 2 in the order they arrive and keeps 1 for `init`, the first process of the system as Linux sees it. A restarted `init` takes id 1 again. The parent of a process is the id in the table of whoever asked for it.

## Limits and known gaps

- 128 rows. A row of a process that exited and was collected stays and is reused only when no row is free; a zombie, which its parent did not collect, and a row whose notice to `rs` is still pending are not reused.
- A process that exits while its parent lives (and is not `rs`) is a zombie until the parent calls `WAIT`. When a process exits, its children pass to `rs`, which collects them; the children of `rs` are collected by `pm` at once.
- No default action for terminate and interrupt: a program that does not read the `SIGNAL` message goes on.
- A forked child is a child of `pm` in the kernel and runs no service. It reaches the name server through `rs`: `pm` sends `PM_FORKED` and the child gets the rights of its parent, nothing more. `rs` also forwards it to the programs that keep state for each process (`notify=fork`), and `pm` holds the reply to the parent until `rs` answers `PM_FORK_SETTLED` (or 40 waits of 50 ms pass), so the copy inherits the descriptors the parent had at the fork.
- A notice that `rs` has no room for is tried again every 50 ms; the inbox of `rs` holds what the kernel allows.
- The boot modules are registered but never marked as exited: their parent is the kernel, which sends no notice.
- `pm` trusts `rs` completely, and trusts the label it is given: it can give a process any program label, so a flaw in `pm` is a flaw in the name server policy. That is the price of keeping it away from hardware.

## Source layout

| File | Holds |
|------|-------|
| `src/main.cpp` | `main`: the grant, then the accept loop that serves calls, takes messages and delivers pending notices |
| `src/grant.cpp`, `include/pm/grant.hpp` | `waitForCapabilities`: blocks until `CAPABILITY_GRANTED`; returns the handle on `rs` and the endpoint of the name server |
| `src/state.cpp`, `include/pm/state.hpp` | The table, the handle on `rs`, who is `ROOT`, `INIT` or `OTHER` (`callerOf`), the labels permitted to `init` |
| `src/objects.cpp`, `include/pm/objects.hpp` | The shared objects `pm` makes for callers |
| `src/spawn.cpp`, `include/pm/spawn.hpp` | `spawnFromObject`: checks the request and creates the process with `cock::user::spawn` |
| `src/requests.cpp`, `include/pm/requests.hpp` | `serve`: one call, who may make it |
| `src/exec.cpp`, `include/pm/exec.hpp` | `execFromObject`: checks an `ExecRequest`, builds the new image as a pending process with the argument page, and asks the kernel to swap it in |
| `src/fork.cpp`, `include/pm/fork.hpp` | `forkProcess`: asks the kernel for a copy of the caller and adds its row; the reply the copy finds is built here. `holdParent`, `settleFork` and `ageSettling`: the reply to the parent is detached until `rs` says the copy is settled |
| `src/wait.cpp`, `include/pm/wait.hpp` | `waitForChild` (collects a zombie or detaches the call) and `answerWaiter` (answers it when the child exits) |
| `src/exit_notice.cpp`, `include/pm/exit_notice.hpp` | `takeMessage` (`PROCESS_EXITED`, and `PM_FORK_SETTLED` from `rs`) and `flushNotices` (`PM_EXITED`, `PM_SPAWNED`, `PM_FORKED`) |
| `src/signal.cpp`, `include/pm/signal.hpp` | `signalProcess`: who may signal whom, then `SYS_PROC_SIGNAL` |
| `Makefile` | `NAME = pm`, `CONSOLE_CLIENT = no` (it prints before `console` exists), a minimal `LIBC_SRCS` |

The table itself is in `shared/include/cock/pm/table.hpp` (host tests in `test/host/test_pm_table.cpp`) and the client in `userland/lib/include/cock/user/pm.hpp`. The test programs `pmclient` (the table and the refusals) and `forkprobe` (a fork, a fork inside the child, each side with its own values, and `wait` for both) check it.

## Documentation

- [pm](../../../documentation/userland/pm.md)
- [rs](../../rs/README.md)
- [init](../../init/README.md)
