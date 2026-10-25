# The process manager, `rs` and `init`

MicroCOCK keeps the process model out of the kernel, as Minix 3 does. Three programs share the work that the `init` of a Unix kernel does, and none of them is in Ring 0:

| Program | Role | Authority |
|---------|------|-----------|
| `rs` | Decides what every program may do, starts the servers and starts them again when they end | The root capabilities: ports, interrupt lines, physical memory, boot modules, the right to grant |
| `pm` | Creates the processes of the system after boot, is their parent in the kernel and keeps the table | The right to create processes and the program labels; no hardware |
| `init` | The first process as Linux sees it (id 1); starts the shell and starts it again | None |

The kernel only offers primitives (`SYS_PROC_CREATE` and its five companions, `SYS_PROC_EXEC`, `SYS_PROC_FORK`). Stage L of the roadmap adds `wait`, `exec`, signals and `fork` as requests to `pm`, with small primitives in the kernel; `wait`, `exec` and `fork` are done, signals come next.

## Boot

1. GRUB loads the kernel and the modules `rs`, `names`, `ramfs`, `vga_driver` and the `initrd`. The first module, `rs`, gets the root capabilities.
2. `rs` brings the name server up, gives `ramfs` the `initrd`, reads `manifest.conf` through `ramfs`, describes the programs to the name server and configures the boot modules (`vga_driver`).
3. `rs` creates `pm` itself: nothing can create it for `rs`. It binds `pm` to its endpoint and to the name server, grants it the right to create processes (the manifest line `pm path=servers/pm caps=spawn`), an `IPC` capability for the program labels, an `ENDPOINT` capability on the endpoint of the name server and a handle on its own endpoint, and sends `CAPABILITY_GRANTED`. It then records `rs`, `pm`, `names` and `ramfs` in the table of `pm` (`REGISTER`), and `vga_driver` when it configures it.
4. `rs` creates every other program of the manifest through `pm` (`console`, `ata`, `rtc`, `fat`, `vfs`, `kbd`, `ext`, then `init`), reading the image straight into an object that `pm` made. After each one it binds the process to the name server and equips it; the kernel parent is `pm`.
5. `init` connects to `pm` and `vfs`, reads `/manifest.conf` and asks `pm` for the programs marked `parent=init` (the shell). `pm` tells `rs` (`PM_SPAWNED`), which registers each one with the name server. A copy made by `fork` is registered the same way with `PM_FORKED`, with the rights of its parent.
6. `rs` keeps serving the notices of `pm` for ever: when a program it watches ends (`PM_EXITED`) it creates it again.

## Why `pm` is not given more

The kernel checks the rights of a process at `SYS_PROC_START`: the creator must hold the right to create processes, a grantable `IPC` capability for the label of the child and for each label of its list, and a grantable `ENDPOINT` capability for the initial handle. `rs` gives `pm` those and nothing else, so no kernel change was needed and `pm` never holds a port or device memory. `rs` keeps granting the hardware after the process exists (`equip`).

The price is trust: `pm` can give a new process any program label, so a flaw in `pm` is a flaw in the policy of the name server. `pm` obeys only the root server for labels; `init` may use only the labels `rs` permitted, with no IPC labels and the initial handle chosen by `pm`.

## Protocol of `pm`

Synchronous calls of four words, `shared/include/cock/abi/pm.hpp`, through a handle whose badge is the process id of the caller.

| Operation | Served for | Meaning |
|-----------|------------|---------|
| `ALLOCATE bytes` | any process in the table | `pm` makes a shared object of that size, shares it with the caller for writing and returns its id; one per caller. Only `rs` and `init` can use it for `SPAWN`; any process can use it for `EXEC` |
| `EXEC object` | any process in the table | The first page of the object holds an `ExecRequest` (name, image size, arguments, the initial handle and the descriptors of the program, which the C library writes and the new image rebuilds; see [descriptors](../libs/descriptors.md)) and the image starts at `IMAGE_OFFSET`. `pm` checks it, builds the new image, writes the argument page and asks the kernel to swap it in (`SYS_PROC_EXEC`). On success the call never returns |
| `FORK` | any process in the table | Makes a copy of the caller. `pm` asks the kernel for it (`SYS_PROC_FORK`) while the caller waits for this very reply, which is the consent the kernel asks for, and adds the row of the child with the caller as its parent. The reply to the caller carries the id in the table of the child; the child, which resumes inside the same call, gets the same reply with id `NO_PID`; the reply to the caller is held until the programs that keep state for each process have copied it. See "Fork" below |
| `KILL pid signal` | any process in the table; `rs` for every process | Sends interrupt, terminate (a `SIGNAL` message the program may ignore) or kill (ends the process, status 137) to a live process; a caller other than `rs` reaches only its descendants. See "Signals" below |
| `SPAWN object` | `rs`, `init` | The first page holds a `SpawnRequest` (name, `SpawnRights`, flags, image size), the image starts at `IMAGE_OFFSET`; `pm` copies the request, checks it and creates the process |
| `REGISTER kernel parent wanted` | `rs` | Records a process that already runs |
| `PERMIT label` | `rs` | `init` may create processes with that label |
| `LIST index` | anyone with a handle | One row of the table, with the state in bits 8 to 15 of the reply header |
| `WAIT pid` | anyone with a handle | Collects the status of a child (`pid` of the table, or `ANY_CHILD`): the reply carries the pid and the exit status. Blocks until a matching child exits; `NOT_FOUND` when there is none |

## Process states

The states are those of `ps` in Linux (`abi::pm::State`). The table stores only two of them, `RUNNING` for a process that lives and `EXITED` (`X`) for one whose exit notice arrived. An `X` row lasts only until its parent collected it with `wait` and the root server got its notices; then `pm` frees the row. A zombie (`Z`) stays until the parent waits. `LIST` does not report the stored `RUNNING` as it is: it asks the kernel for the states of the live threads (`SYS_PROC_STATE`) and `cock::pm::classify` (`shared/include/cock/pm/classify.hpp`) turns the mask into `R` when a thread is ready or running and into `S` when every live thread waits (timer, futex, message, accept or reply); a stopped process is `T` whatever its threads do (the kernel adds `THREAD_STOPPED` to the mask). Nothing is reported by the scheduler and nothing is stored, so the thread path of the kernel has no extra work, and the answer is the state at the moment of the call. When the kernel cannot read the threads, for example because the first thread of the process was already reaped while another thread lives, `LIST` answers `R`.

`Z` (zombie) is stored: a process that exits while its parent lives, and is not the root server, stays `Z` with its id and its status until the parent collects it with `WAIT`; then it is `X`. `T` (stopped) is not stored either: the kernel knows it, as it knows `S`, and `LIST` reports it. `D` (uninterruptible sleep) has a value in `State` and a letter in `ps`, and nothing produces it: it comes with the first wait that a signal must not end, which sets it and says so in its header.

### Wait and adoption

`WAIT` is a synchronous call that reaches only the children of the caller. When a child is a zombie the reply is immediate and the row is collected: it becomes `X` for an instant and `pm` frees it at the end of the request (`Table::sweep`), so `ps` no longer lists it. When the children still run, `pm` detaches the call (`ipc_defer`), keeps its ticket in the row of the caller and goes on serving; when the exit notice of a matching child arrives, `pm` answers with `ipc_reply_to` and collects it. A caller blocked in `WAIT` that dies is dropped by the kernel; its row loses the wait when its own exit notice arrives. The kernel does not change: the exit notice, the freeing of memory and threads, and the deferred reply already existed.

When a process exits, its children pass to the root server (the reaper, `rs`, set when `rs` registers itself), and those that were zombies become `X`, since `rs` collects them. The children of `rs` are never zombies: `rs` already learns about them through `PM_EXITED`, so `pm` collects for it and `WAIT` from `rs` answers `NOT_FOUND`. `init` collects with `wait` each program of the manifest that it started and that ended, so no zombie stays.

### Fork

`Client::fork()` (`userland/lib/include/cock/user/pm.hpp`) sends `FORK` and returns a `Forked`: in the parent `OK` and the id in the table of the child, in the child `OK` and `NO_PID` (`isChild`). The two sides share their memory copy on write ([memory management](../core/memory_management.md#copy-on-write)), so each sees its own values after a write; the child has one thread, the one that called. It keeps the `IPC` and endpoint capabilities of the parent with the same handle numbers, so the `Client` it copied still reaches `pm`, and a handle whose badge is the parent's id carries the child's ([capabilities](../core/capabilities.md#inheritance-by-fork)); servers therefore see two callers. The ones that keep state for each process (`vfs` and `pipe`, marked `notify=fork` in the manifest) copy the open files and the pipe ends of the parent for the child under the same numbers, so **the descriptors the child inherited work** ([descriptors](../libs/descriptors.md)). Hardware capabilities and `SPAWN` stay with the parent, and the child has no service label.

`pm` refuses `FORK` for a caller with no live row (`NOT_FOUND`), when the table is full (`NO_ROOM`, before the kernel is asked) and when the kernel refuses (`FAILED`: no process id, no memory for the page tables, too many children with a pending exit notice, a shared object with no holder slot, no room in the derivation tree). The child is a child of `pm` in the kernel, so its exit notice reaches `pm`, and a child of the caller in the table, so the caller collects it with `WAIT`; when the parent exits first the child passes to `rs` like any other. Right after it adds the row, `pm` sends `rs` a `PM_FORKED` message (tried again every 50 ms while the inbox of `rs` is full), `rs` tells the name server, and the child may then connect by name to what its parent may and to nothing else; a connect it makes before that is kept by the name server until `rs` registers it, up to `CONNECT_WAIT_MS`. `pm` holds the reply to the parent (`ipc_defer`) until `rs` answers with `PM_FORK_SETTLED`, which `rs` sends after it told the programs with `notify=fork` and the name server, or until 40 waits of 50 ms have passed. So the parent cannot open, close or write anything between the fork and the copy of its descriptors, which is what makes `pipe(); fork(); close(end)` safe, and the copy, which runs at once, first connects to `/run/forked` (`abi::pm::FORK_SETTLE_NAME`), a name nothing has, only to wait until the name server knows it (`Client::fork()` does this). Threads of the parent other than the caller are not stopped, so what they do meanwhile is not covered.

The size of the object is `pm`'s own, so a caller cannot make `pm` read past it. `pm` checks that the name ends, the flags are known (`FORWARD_EXIT`, `AS_INIT`), the image fits the object, the label is a program label and there are at most sixteen IPC labels. A process created for `init` gets no IPC label and an initial handle on the name server.

Two notices travel the other way, as ordinary messages through a handle that `rs` gave `pm`; the kernel stamps the badge, which is the process id of `pm`, and `rs` ignores any other:

| Message | Meaning |
|---------|---------|
| `PM_EXITED` | `arg0` kernel process id, `arg1` exit status: a process created with `FORWARD_EXIT` ended |
| `PM_SPAWNED` | `arg0` kernel process id, `arg1` label: a process that `init` asked for exists |
| `PM_FORKED` | `arg0` kernel process id of a copy made by `FORK`, `arg1` that of the process it copies; `rs` tells the name server, so the copy has the rights of its parent |
| `PM_FORK_SETTLED` | From `rs` to `pm`, the other way: `arg0` kernel process id of the copy; `rs` sends it after it forwarded `PM_FORKED` to the programs with `notify=fork` and registered the copy with the name server, and `pm` then answers the parent. The badge is the process id of `rs`, which `pm` checks |

A notice that finds the inbox of `rs` full is tried again every 50 ms.

### Signals

`Client::kill(pid, signal)` sends `KILL` with the id in the table of the target and one of five signals (`abi::process`, Linux numbers): `SIGNAL_INTERRUPT` (2), `SIGNAL_TERMINATE` (15), `SIGNAL_KILL` (9), `SIGNAL_STOP` (19) and `SIGNAL_CONTINUE` (18). **Who may signal whom is decided here**: the root server may signal any process that lives, and any other caller only its descendants (`Table::isDescendant`: its children, theirs, and so on, following the parent of each row, so a process whose parent exited first belongs to `rs`). A caller never reaches itself, its parent or a sibling, and `pm` answers `DENIED`. The target must be a live row: a process that ended, or is a zombie, is `NOT_FOUND`; a signal other than the five is `BAD_REQUEST`.

The delivery is the kernel's (`SYS_PROC_SIGNAL`, which `pm` may call because it holds `SPAWN`; any other program gets a refusal from the kernel itself). Interrupt and terminate are an ordinary message: the kernel puts a `SIGNAL` message with the signal in `arg0` in the inbox of the main thread, and **the program decides what to do with it**; one that never reads its inbox is not affected, and one that wants to finish reads it and returns from `main`. Nothing in the message can be forged, because the kernel refuses to send the type `SIGNAL` on behalf of a program. Kill cannot be caught: the kernel ends every thread of the process, whatever each one waits for, and the process exits with `KILLED_STATUS` (137, 128 plus the signal as a shell shows it), so the exit notice reaches `pm`, the row becomes a zombie, and the parent collects that status with `WAIT`. The row does not change when the signal is sent but when the notice arrives. `FAILED` means the kernel refused: the inbox of the target is full, or the target is the process of the caller.

Stop and continue are the kernel's too, and no message is sent. **Why the kernel:** only the scheduler can keep a thread from running, and a thread that wakes up while its process is stopped (a message, a timer, a futex, a reply) must not be queued; a server cannot do that from outside. The mark is one flag of the process (`Process::isStopped`): `TaskManager::stopProcess` sets it and takes the queued threads off the ready queues, and from then on `Scheduler::enqueue` and the direct handoff of a call skip every thread of the process. A thread that wakes becomes ready and waits there; `continueProcess` clears the flag and queues the ready ones, so the work that arrived meanwhile is done. A stopped process can still be killed, stop and continue succeed when the process is already in that state, and the parent is not told (there is no `WUNTRACED`). `LIST` shows the row as `T` while it lasts; the table stores nothing, and an `exec` that finishes while the process is stopped leaves it stopped.

## The table

The id in the table is not the kernel one. It counts from 2 in the order processes arrive; id 1 is `init`, and a restarted `init` takes it again. The parent is the id in the table of whoever asked for the process: `rs` for the servers, `init` for the shell. A row has the kernel id, the id, the parent, the state (running, zombie or exited) and the exit status; the row of a process that exited and was collected stays until no row is free, and a zombie row is never reused. The table is a header (`shared/include/cock/pm/table.hpp`) with host tests.

## Limits

- No signal handlers table (a program reads the `SIGNAL` message itself) and no default action for terminate and interrupt: a program that does not read them goes on. Any process can fill the 8 object slots of `pm` with `ALLOCATE` and starve `spawn` until a permission model exists (E5). A parent that never calls `WAIT` leaves zombies that fill the table (as in Linux) until it exits; `WAIT` has no non-blocking flag and no timeout.
- If `pm` dies the table and the notices of its children are lost, because their parent disappears. `rs` could start another one but not rebuild the table. Minix has the same limit.
- The boot modules are in the table but are never marked as exited: the kernel sends no notice for them.
- A process may have at most `MAX_NOTIFIED_CHILDREN` (64) children with a pending exit notice, and `pm` has 128 rows.
- `init` still polls `process_alive` every 250 ms to keep the shell, and calls `wait` once for each program it finds ended; a loop that blocks in `wait` instead of polling needs a way to wait for a child and a timer at once.

See also the READMEs of [`pm`](../../userland/servers/pm/README.md), [`rs`](../../userland/servers/rs/README.md) and [`init`](../../userland/servers/init/README.md), [capabilities](../core/capabilities.md) and [the name server](names.md).
