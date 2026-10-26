# Userland

Everything that is not the kernel runs in Ring 3 as a program loaded from a Multiboot module. This page describes the programs that ship with the system, how they are built and started, the system call ABI, and how to write a new one.

## Programs

The test initrd is `test/initrd/` (manifest lines, the `/mute` mount and the fixture files `hello.txt` and `docs/note.txt`); only `scripts/qemu_userland_test.sh` builds with it. The programs that exist only to test the system (`probe`, `hello`, `aborter`, `syncbench`, `contruncated`, `fuzz`, `fsclient`, `fatclient`, `extclient`, `blkclient`, `cfile`, `mute`, `muteclient`) live in `test/userland/`, not in `userland/`. Their Makefiles include `../../../userland/common.mk` and the root Makefile builds both folders into `build/userland`. The helper `write_check.hpp` they share is in `test/userland/lib/include/cock/user/`.

| Program | Role |
|---------|------|
| `rs` | Root server. The only process born with authority; it grants `ramfs` the `initrd`, reads `manifest.conf` through `ramfs` and does what each line says (asks `pm` to start the programs, grants capabilities) and then serves the notices of `pm` for ever, starting the servers again when they end; see [pm](pm.md) |
| `pm` | Process manager. `rs` creates it from the initrd and it creates every other process after it; it is their parent in the kernel and keeps the table; see [pm](pm.md) |
| `init` | The first process as Linux sees it (id 1), without authority: starts the programs of the manifest that carry `parent=init` (the shell) and starts them again; see [pm](pm.md) |
| `pmclient` | Test program: lists the table of `pm`, checks the first rows, its own row and that a caller that is not `rs` is refused; the end to end test requires `pmclient: ok` |
| `sigprobe` | Test program: signals through `pm` (`ipc=pm,console`). A forked child catches terminate and exits with its own status, a second child ignores terminate and interrupt and is ended by kill (status 137), a signal to an ended process is `NOT_FOUND`, an unknown signal is `BAD_REQUEST`, a child may not signal its parent (`DENIED`), a stopped child shows `STOPPED` in `LIST`, does not run (its timer expires meanwhile) and finishes after continue, a stopped child can be killed, and a program can neither call `SYS_PROC_SIGNAL` nor send a `SIGNAL` message itself; the end to end test requires `sigprobe: ok` |
| `forkprobe` | Test program: forks through `pm`, changes a global and a stack variable on each side and checks that each keeps its own value, forks again inside the child (which only works through the handle it inherited), connects the child and the grandchild through the name server (`console` allowed, `vfs` refused, as for the parent), and collects both with `wait`; the end to end test requires `forkprobe: ok` |
| `names` | Name server, the second boot module. `rs` describes the system to it; a program asks it, through the handle it was born with, for a handle on a server; see [names](names.md) |
| `vga_driver` | Display server. Maps the video memory and draws what it receives by IPC |
| `ramfs` | File server for the `initrd`; see [file system](filesystem.md) (a boot module, so `rs` can read the manifest) |
| `console` | Console driver: owns the serial port `0x3F8+8`; serves `WRITE`, `CLEAR` and `ECHO` as synchronous calls and draws on the display server; no line logic; see [console](console.md) |
| `tty` | Terminal server: the typed line, its editing, the finished lines and the readers that wait for them; no capabilities; the echo goes through `console`; see [tty](tty.md) |
| `fuzz` | Syscall fuzzer, only started with `FUZZ=user` (from the manifest) or `FUZZ=root` (as the module `rs`); see [fuzzer](fuzzer.md) |
| `contruncated` | Test program: sends a text to `console` without the closing chunk and exits, to check that the server shows what a dead program had sent |
| `kbd` | PS/2 keyboard driver. Binds IRQ 1, reads port `0x60`, decodes the scan codes and sends characters to the `tty` server |
| `sh` | The shell: prompt, a line read from `tty`, and the built-in commands `help`, `cd`, `pwd` and `exit`; every other command is a program of `/bin` that it runs with `fork`, `exec` and `wait`; see [shell](shell.md). Started last by the system manifest |
| `uname` | Prints `microcock vMAJOR.MINOR.PATCH`. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](shell.md) |
| `true`, `false`, `date` | Exit status 0, exit status 1, and the date and time from the `rtc` server. Programs of `/bin` that the shell runs, with no manifest line and no capabilities (`date` uses the `rtc` right that the shell passes through `exec`); see [shell](shell.md) |
| `echo` | Prints its arguments. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](shell.md) |
| `ls` | Lists a directory, through the `vfs` client. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](shell.md) |
| `cat` | Prints files. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](shell.md) |
| `mkdir` | Makes directories. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](shell.md) |
| `rmdir` | Removes empty directories. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](shell.md) |
| `rm` | Deletes files. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](shell.md) |
| `mv` | Renames or moves inside one volume. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](shell.md) |
| `ps` | Lists the processes of `pm`. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](shell.md) |
| `sleep` | Waits a number of seconds. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](shell.md) |
| `clear` | Clears the screen through `console`. A program of `/bin` that the shell runs, with no manifest line and no capabilities; see [shell](shell.md) |
| `probe` | Security probe, split by topic in `security.cpp`, `memory.cpp`, `ipc.cpp`, `output.cpp`, `heap.cpp`, `shared_memory.cpp`, `files.cpp` and `threads.cpp`, `thread_library.cpp`, `sync.cpp` and `startup.cpp`, with the shared constants in `probe.hpp`. Checks from Ring 3 that the kernel denies what it must, and prints `probe: 54/54 checks passed` |
| `hello` | Assembly program that writes a line and exits |
| `fsclient` | Reads a file from the `initrd` and exits; `rs` starts it ten times, one after the other, so the server has to release the sessions of the ones that died |
| `aborter` | Calls `abort()` so the end to end test sees its message and the process being reaped |
| `rtc` | Clock server, the only owner of the CMOS ports `0x70+2` (granted by the manifest); answers `RTC_READ` with the date and time read twice until two reads agree, so two servers can never interleave on the index port; started before `fat` and `ext` |
| `pipe` | Pipe server: holds the pipes and the ends of each process, serves `CREATE`, `READ`, `WRITE` and `CLOSE` as synchronous calls and holds a reader or writer with `SYS_IPC_DEFER` while the pipe is empty or full; no capability; see [pipe](pipe.md) |
| `ata` | Block server for the first IDE disk: PIO reads, MBR and GPT partitions; started by `rs` with the ports `0x1F0+8` and `0x3F6+1`; see [block storage](block_storage.md) |
| `fat` | FAT32 server over the block protocol; see [FAT32](fat32.md) |
| `vfs` | The file server clients talk to: mount table of `/` (`ramfs`) and `/fat` (`fat`); see [file system](filesystem.md#the-vfs-server) |
| `fatclient` | Reads files, a long name, a nested file, a multi-cluster file and a listing from `/fat`, and checks the refusals; the end to end test requires its output |
| `ext` | ext2 server over the block protocol; the manifest also starts it as `ext4probe` on the ext4 volume to show that it is refused; see [ext2](ext2.md) |
| `extclient` | Reads files at every level of the block map, a sparse file, listings, symbolic links and the refusals from `/ext`; the end to end test requires its output |
| `cfile` | Uses only the C library (`open`, `read`, `write`, `lseek`, `close`, `dup`, `dup2`, `unlink`) on files of `/ext` and `/fat` (and checks that `dup` shares the offset, that `dup2` replaces a descriptor and that standard output can be redirected to a file): create, append, overwrite after a seek, truncate, delete and the refusals; it checks `errno` for each refusal, makes, renames and removes directories, and that a message sent to itself before a file call is still there afterwards; it leaves `cfile.txt` for the host to check |
| `pipeclient` | Uses only the C library (`pipe`, `read`, `write`, `dup`, `dup2`, `close`) and the pipe client: a round trip, the end of the data, `EPIPE`, an end kept open by `dup`, standard output redirected into a pipe, a reader woken by a late write from another thread, a writer held by a full pipe, and the refusals of the server; prints `pipeclient: ok` |
| `descprobe`, `descchild` | `descprobe` forks and checks that the child uses the descriptors it inherited: a pipe end (the parent closes its own end right after the fork), an open file with the offset of the parent, standard output redirected to a pipe, and, after an `exec` into `descchild`, the same pipe and file in the new image; prints `descprobe: ok` |
| `mute` | Test server: reads one message and exits without answering |
| `muteclient` | Checks that requests to a dead server fail instead of hanging, through `vfs` and directly |
| `blkclient` | Lists the devices of `ata` and reads the first sector of the first partition; the end to end test checks its output |

The processes see each other through the name server: a program calls `cock::user::names::connect("kbd")` and gets the process id of the server and, if the manifest allows it, the right to talk to it. The kernel finds a process by name only for `rs`, and only among the boot modules (`SYS_MODULE_PROCESS`). See [names](names.md).

## How the system starts the userland
1. The Makefile builds every `userland/*/*/Makefile` into `build/userland/<name>.elf`. With `TEST_INITRD=1` it also builds `test/userland/*/Makefile`.
2. It packs the files of the `initrd/` folder and every program of those folders that is not a boot module (under `drivers/` and `servers/` those of `userland/drivers/` and `userland/servers/`, under `bin/` the applications and the test programs, each without the `.elf` suffix) into `build/initrd.tar` (ustar). With `TEST_INITRD=1` the files of `test/initrd/` are added, and the `manifest.conf` and `mounts.conf` of the two folders are joined (system lines first), so a normal `make iso` boots the system with no test program and `make iso TEST_INITRD=1` boots the same system plus the tests. It writes a `grub.cfg` with one `module` line for each program of `USERLAND_BOOT_ORDER` (`rs`, `names`, `ramfs`, `vga_driver`), and a last line for the archive, named `initrd`. The archive is a data module (see [memory management](../core/memory_management.md)), so it never becomes a process.
3. The kernel loads each module as a process (`rs`, `names`, `ramfs`, `vga_driver`) and gives the root capabilities to the first one, `rs`. `rs` creates the process manager `pm` itself and every other program through it; `init` starts the programs marked `parent=init` (see [pm](pm.md)).
4. `rs` labels `names` and gives it the right to pass on `IPC` capabilities, registers `ramfs`, grants `ramfs` the `initrd` and sends it `CAPABILITY_GRANTED`; this is the only policy written in its code. It then reads `manifest.conf` through `ramfs`, parses it, sends the programs and the `ipc=` policy to the name server, and runs each line in order (see below). A spawned process is born with no capabilities, so the grants of its line are what it gets. `vga_driver` maps the screen, and `kbd` enables the port and binds the interrupt.
5. A key press travels: keyboard controller, IRQ 1, kernel (`IRQ_NOTIFICATION`), `kbd` reads the scan code and acknowledges the line, `kbd` sends `CONSOLE_KEY` to `tty`, `tty` edits the line and asks `console` for the echo, and `console` sends it to `vga_driver`, which draws it.

A driver that needs a new capability needs a new `cap_grant` in `userland/servers/rs/src/main.cpp`; see [capabilities](../core/capabilities.md). The display server is described in [VGA](vga.md) and the keyboard driver in [keyboard](keyboard.md).

## The manifest
`manifest.conf` in the initrd says which programs run and what authority each one gets. It is plain text, one entry per line; empty lines and lines that start with `#` are ignored. The parser is `shared/include/cock/abi/manifest.hpp`, which the host tests cover.

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
| `parent=` | Only `init`: `init`, not `rs`, starts the program, through `pm`. Needs `path=`, one copy and no `restart=`. See [pm](pm.md) |
| `owner=` | The name of the program that owns this one. With `mode=` it decides who may connect to this program through the name server, besides the `ipc=` lists. See [names](names.md) |
| `mode=` | Two characters, `x` to allow and `-` to refuse: the first for the owner, the second for every other program (`x-` only the owner, `-x` everybody but the owner, `xx` the default). A refused `connect` answers `DENIED`. See [names](names.md) |
| `notify=` | Only `fork`: the program keeps state for each process (`vfs` its open files, `pipe` the ends of pipes), so `rs` sends it a `PM_FORKED` message for every copy made by `fork`, and it gives the copy what its parent had. Needs `path=`. See [pm](pm.md) |

When an entry has grants or an argument, `rs` sends the process `CAPABILITY_GRANTED` after granting. The kernel still checks every grant against what `rs` holds, so a line cannot give more than `rs` has. The parser rejects the whole file at the first bad line (unknown key, repeated key, bad number, range that wraps the address space, too many entries or grants, repeated name) and `rs` then starts nothing and exits; the serial log shows the line number and the error. The file is at most 4096 bytes and 32 entries. Adding a driver means adding a line here, not editing `rs`.

## The `cock::user` API
`userland/lib/include/cock/user/api.hpp` wraps the system calls for the programs written in C++:

| Function | System call |
|----------|-------------|
| `ipc_send(pid, message)`, `ipc_recv(message)` | `SYS_IPC_SEND` (to a thread of the caller's own process; another program is reached with `ep_send`), `SYS_IPC_RECV`; `ipc_recv` returns the messages stashed by the C library first |
| `request_ports(first, count)` | `SYS_REQUEST_RESOURCE` |
| `irq_bind(line)`, `irq_ack(line)` | `SYS_IRQ_BIND`, `SYS_IRQ_ACK` |
| `cap_grant(pid, type, first, count, delegate)` | `SYS_CAP_GRANT`; `delegate` lets the receiver pass the capability on |
| `names::connect(name)`, `names::server(name)` (`names.hpp`) | A synchronous call to the name server: it checks the `ipc=` policy, gives the caller an `IPC` capability for the server and answers with its process id; `connect` returns the status and the id, `server` only the id (`NO_PROCESS` when refused) |
| `thread_process(tid)` | `SYS_THREAD_PROCESS`; the process a thread belongs to |
| `module_process(name)` | `SYS_MODULE_PROCESS`; the process of a boot module, for the root server |
| `shm_create(pages)`, `shm_share(id, pid, writable)`, `shm_map(id, writable)`, `shm_release(id)` | `SYS_SHM_CREATE`, `SYS_SHM_SHARE`, `SYS_SHM_MAP`, `SYS_SHM_RELEASE` |
| `port_out8(port, value)`, `port_in8(port)`, `port_read_words(port, buffer, words)` | Port instructions (`outb`, `inb`, `rep insw`), allowed after `request_ports` |
| `module_open(name)` | `SYS_MODULE_OPEN` |
| `module_index(name)` | `SYS_MODULE_INDEX` |
| `spawn(image, size, name, notify_exit, rights)` (`spawn.hpp`) | The loader in user space: reads the ELF image and builds the process with `SYS_PROC_CREATE`, `SYS_PROC_MAP`, `SYS_PROC_WRITE`, `SYS_PROC_PROTECT` and `SYS_PROC_START`, and destroys it with `SYS_PROC_DISCARD` if a step fails; `notify_exit` asks for a `PROCESS_EXITED` message when the process ends; `rights` (optional) gives the new process its service label, `IPC` capabilities for labels and one initial handle (whose number the program reads with `cock::user::initialHandle()`) before it runs |
| `service_set(pid, label)` | `SYS_SERVICE_SET`; labels a running process once (used for boot modules) |
| `cap_accept(mask)`, `cap_send(pid, type, first, count, delegate, tag)` | `SYS_CAP_ACCEPT`, and `SYS_IPC_SEND` of a `CAPABILITY_TRANSFER` message (returns the node id of the copy, `abi::NO_CAPABILITY_NODE` on failure) |
| `cap_grant_node(pid, type, first, count)`, `cap_revoke(node)` | `SYS_CAP_GRANT` returning the derivation node, and `SYS_CAP_REVOKE` |
| `process_alive(pid)` | `SYS_PROC_ALIVE` |
| `process_state_mask(pid)` | `SYS_PROC_STATE` |
| `proc_signal(pid, signal)` | `SYS_PROC_SIGNAL`; only `pm` holds the capability. Programs use `pm::Client::kill` |
| `proc_exec(handle, entry, pid, initial)` | `SYS_PROC_EXEC`; only `pm` holds the capability |
| `proc_fork(pid, reply)` | `SYS_PROC_FORK`; only `pm` holds the capability. Programs use `pm::Client::fork` |
| `exec(process, files, path, argv, argc)` (`exec.hpp`) | Replaces the image of the program with a file: reads it through the file server into an object of `pm` and sends `EXEC`; returns only when it failed (`ExecFailure`) |
| `argumentCount()`, `argument(index)` (`crt0.hpp`) | The arguments of a program that `exec` started, from the page at `abi::exec::ARGS_ADDRESS`; none for a program that `spawn` started. `_start` also passes them to `main`: a program may define `extern "C" int main(int argc, char **argv)` (`argv[argc]` is null) or `main()`, which ignores them |
| `ipc_try_recv(message)` | `SYS_IPC_TRY_RECV`; `Received::MESSAGE`, `EMPTY` or `FAILED` |
| `ipc_recv_timeout(message, milliseconds)` | `SYS_IPC_RECV_TIMEOUT`; the same results, `EMPTY` when the time ran out; like `ipc_try_recv` it reads only the kernel inbox |
| `sleep_ms(milliseconds)` | `SYS_SLEEP`; true when it slept (resolution 10 ms) |
| `thread_start(function, argument, stack, size)` (`thread.hpp`), `thread_create_raw`, `gettid()` | `SYS_THREAD_CREATE` and `SYS_GETTID`; the program provides the stack; the thread function is `void f(void *)` and may return |
| `ipc_call(target, words)` (a thread of the caller's own process), `ep_call(handle, words)` (another program), `ipc_accept()`, `ipc_reply(words)`, `ipc_reply_wait(words)` (`call.hpp`) | Synchronous calls in registers, four words each way, an optional time in milliseconds, `CallStatus` `OK`, `TIMED_OUT`, `MESSAGE_PENDING` or `FAILED`; see [ipc](../core/ipc.md#synchronous-calls) |
| `futex_wait(word, expected, ms)`, `futex_wake(word, count)`, `Mutex` (`mutex.hpp`), `ConditionVariable` (`condition_variable.hpp`), `Semaphore` (`semaphore.hpp`), `JoinableThread` with `join` (`joinable_thread.hpp`) | `SYS_FUTEX_WAIT` and `SYS_FUTEX_WAKE`; the last three are built on them and on `process_alive`, with no kernel change; see [task management](../core/task_management.md#threads) |
| `Clock` (`clock.hpp`), `init()`, `now(time)` | Asks the `rtc` server for the date and time (a program needs no port capability; `onForeign` receives the messages that arrive while it waits); `abi/datetime.hpp` converts it to Unix seconds and to FAT date and time. `Cmos` (`cmos.hpp`) is the reader of the ports `0x70` and `0x71` that only `rtc` uses |
| `self()` | `SYS_GETPID` |
| `write_text(text, length)` | `SYS_WRITE`, repeated until the whole text is written (a call takes at most 128 bytes) |
| `exit_process(status)` | `SYS_EXIT` |
| `port_in8(port)` | The `in` instruction (the process needs the port enabled) |

`mmap` and `munmap` are in the C library (`<sys/mman.h>`).

## System call ABI (x86)
| Register | Content |
|----------|---------|
| `EAX` | System call number on entry, result on return (`-1` is failure) |
| `EBX`, `ECX`, `EDX`, `ESI`, `EDI` | Arguments 1 to 5 |
| `EBP` | Argument 6 |

All the other registers are preserved. `libc/include/sys/syscalls.h` has the inline `syscall()` that does the call.

Programs cannot use floating point, MMX or SSE: the kernel sets `CR0.EM`, so those instructions fault and the kernel terminates the process. The programs are compiled with `-mno-sse -mno-mmx -mno-80387` so the compiler never emits them.

### Why the wrapper looks the way it does
The sixth argument has to travel in `EBP`, which compilers often use as the frame pointer, and a compiler is free to address the local variables of a function through it. An inline assembly block that loads `EBP` and then reads one of its own operands from memory can end up reading from `(EBP - n)` with the new value of `EBP`, and fault. This happened: programs built by a compiler that keeps frame pointers by default died on their first system call with a page fault close to address 0.

The wrapper avoids it. The fifth and sixth arguments are stored in a two-element array on the stack, `EDI` points to it, and the assembly is:

```cpp
push %ebp
mov 4(%edi), %ebp
mov (%edi), %edi
int $0x80
pop %ebp
```

No memory operand is addressed through `EBP` or the stack pointer, so it works with and without frame pointers. If you change it, build the userland with `-fno-omit-frame-pointer` as well and run the end to end test.

### Blocking calls and `int 0x80`
A call that has to wait is restarted: the kernel rewinds the instruction pointer to the `int $0x80` and runs the call again when the thread wakes up, with the registers as they were. The wrapper does not notice, except that the call takes longer.

## Building a program
A program needs a folder in `userland/` with a `Makefile`:

```make
NAME = hello_c
SRCS = main.cpp
LIBC_SRCS = string/memcpy.cpp

include ../common.mk
```

* `NAME` is the name of the ELF and **the name the kernel gives the process**.
* `SRCS` are the sources of the program.
* `LIBC_SRCS` picks the files of `libc/` to link with the program; there is no shared library.
* `printf`, `puts` and `putchar` need `stdio/printf.cpp`, `stdio/puts.cpp`, `stdio/putchar.cpp`, `unistd/write.cpp` and `string/strlen.cpp` (plus `string/memset.cpp`). `printf` formats into a 1 KiB buffer and writes it with a loop of `write` calls, because the kernel cuts each `SYS_WRITE` at 128 bytes; it supports `%d %u %x %o %p %c %s` and `%%`, without width or precision. It returns the number of characters written, or `-1` if a write failed. `putchar` is one system call per character, so prefer `printf` and `puts`.
* `malloc`, `calloc`, `realloc` and `free` need `stdlib/malloc.cpp`, and `operator new` and `operator delete` need `LIBCXX_SRCS = memory/user_new.cpp` (plus `string/memcpy.cpp`, `string/memset.cpp` and the `mmap` file). The allocator keeps a free list ordered by address, merges neighbouring free blocks and serves requests first fit with 8 byte alignment. It grows by anonymous `mmap` chunks of 16 pages that double up to 256 pages, or by the request rounded to pages when that is larger. Chunks are never returned, because `munmap` needs the exact region. When the process quota refuses a chunk, `malloc` returns a null pointer and the heap stays usable. `realloc` never shrinks in place and moves the block when it grows. Every entry point takes one futex mutex (`cock::user::Mutex`, so the uncontended path is one atomic operation and no system call), which makes the allocator safe for the threads of one process; `calloc` takes it through `malloc`, and `realloc` holds it for its whole move. `operator new` calls `abort` when memory runs out, because the build has no exceptions. `abort` (`stdlib/abort.cpp`, with `unistd/_exit.cpp` and `unistd/write.cpp`) writes `abort()` to the debug output and ends the process with exit status 134 (128 plus `SIGABRT`) through `_exit`. The kernel does not build these two files: it has its own heap.
* **Files.** `open` (`<fcntl.h>`: `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_TRUNC`, `O_APPEND`), `read`, `write`, `lseek`, `close`, `unlink`, `rmdir` and `rename` (`<unistd.h>`) and `mkdir` (`<sys/stat.h>`) go through the file client of `vfs`. A program that uses them lists `unistd/file.cpp`, `unistd/errno.cpp`, `unistd/read.cpp` and `unistd/write.cpp` (plus `string/memcpy.cpp`, `string/memset.cpp`, `string/strlen.cpp`). Descriptors 0 to 2 are the console: `write` goes to `SYS_WRITE` as before and `read` goes to `SYS_READ`, which is still the stub for standard input (there is no console server). Descriptors from 3 up (16 at most per program) are files: the first `open` connects to `vfs` (a window of one page), and every call is one request, cut to the window, so `read` can return less than asked and `write` loops until everything is written or the volume is full. The offset lives in the library, `O_APPEND` writes at the end of the file, and a seek past the end is accepted but a write there is refused by the server (no holes). `O_RDWR`, `O_WRONLY`, `O_CREAT` and `O_TRUNC` open for writing, which is exclusive (see [writing](filesystem.md#writing)), and `O_CREAT` needs a write mode. A failure returns -1 and sets `errno` (`<errno.h>`, one global variable, no threads): `ENOENT`, `EEXIST`, `EBADF`, `EINVAL`, `EMFILE` (16 descriptors), `ENOSPC`, `EROFS`, `EBUSY`, `ENOTEMPTY`, `EXDEV`, `EIO`, `ENOTSUP` and `ENOTCONN` come from the status of the server. `write` and `read` reach the file layer through weak symbols, so a program that only prints does not link the client. While a call waits, the library keeps every message that is not the awaited reply in a stash of 16 (`stash_message` in `api.hpp`), and `ipc_recv` hands the stashed messages out, in order, before it asks the kernel; `ipc_try_recv` reads only the kernel inbox. A stash that is full drops the newest message.
* The entry point of a program is `extern "C" int main()`, without arguments. The program does not write `_start`: the shared `crt0` (`userland/lib/src/crt0.cpp`, linked into every program by `common.mk`) is the ELF entry. It runs the static constructors (`cock::user::runConstructors`, over the `.init_array` that `userland/linker.ld` brackets with `__init_array_start` and `__init_array_end`), calls `main` and ends the process with `exit_process` and the value `main` returned. A server that loops forever never returns from `main`, and a program may also end with `exit_process` itself. Nothing else is set up before `main`, so a constructor must not use a server that has not been granted yet.
* `common.mk` compiles freestanding, without exceptions, RTTI, stack protector or FPU code, links at `0x400000` with `userland/linker.ld`, and tracks the headers each object includes, so changing a header rebuilds what depends on it.
* `USER_EXTRA_FLAGS` adds compiler flags for every program, for example `make iso USER_EXTRA_FLAGS=-fno-omit-frame-pointer`.
* `userland/linker.ld` produces two loadable segments: code and constants (read and execute) and data (read and write). The kernel seals the first one read only when it loads the program. A program without data has an empty second segment, which the loader accepts.

The main Makefile passes the compiler, linker and assembler (`USER_CXX`, `USER_LD`, `USER_AS`) so the userland is built with the same toolchain as the kernel. When building one program by hand, pass them yourself.

The programs run in a user-space limit of `0xC0000000` (ELF32). Their stack is the top 64 KiB below that limit, and `mmap` hands out addresses from `0x60000000` upward.

## Testing
* `bash scripts/qemu_userland_test.sh` boots the ISO under QEMU, types `abCd` on the simulated keyboard through the QEMU monitor and requires: the `hello` line and `probe: 54/54 checks passed` on the screen, the typed text on the screen, the lines of the block driver and its client (see [block storage](block_storage.md#tests); the script attaches a test disk and accepts `DISK_SCHEME=gpt`), the `printf` sample line `probe: printf -42 4294967295 FF hi Z 0xBEEF` and the `abort()` message of `aborter` on the serial log, `kbd: ready`, the VGA driver detection and the reaping of the processes that exit on the serial log, and not one exception.
* Run it also with `USER_EXTRA_FLAGS=-fno-omit-frame-pointer`, which changes how the compiler addresses locals and caught a real bug in the system call wrapper.
* Both variants run in CI, on every push.
* `bash scripts/host_test.sh` builds `test/host/` with the host compiler and runs it; it needs no emulator and checks the ring buffer, the intrusive queue, the linked list, the message layout, the capability table, the manifest parser, the mount table parser, the partition table parsers, the FAT32 parser and the ext2 parser.
