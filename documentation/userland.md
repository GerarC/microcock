# Userland

Everything that is not the kernel runs in Ring 3 as a program loaded from a Multiboot module. This page describes the programs that ship with the system, how they are built and started, the system call ABI, and how to write a new one.

## Programs

| Program | Role |
|---------|------|
| `init` | Root server. The only process born with authority; it grants `ramfs` the `initrd`, reads `manifest.conf` through `ramfs` and does what each line says (starts programs with `SYS_SPAWN`, grants capabilities), then exits |
| `vga_driver` | Display server. Maps the video memory and draws what it receives by IPC |
| `ramfs` | File server for the `initrd`; see [file system](filesystem.md) (a boot module, so `init` can read the manifest) |
| `kbd` | PS/2 keyboard driver. Binds IRQ 1, reads port `0x60`, decodes the scan codes and sends characters to the display server |
| `probe` | Security probe, split by topic in `security.cpp`, `memory.cpp`, `ipc.cpp`, `output.cpp`, `heap.cpp`, `shared_memory.cpp` and `files.cpp`, with the shared constants in `probe.hpp`. Checks from Ring 3 that the kernel denies what it must, and prints `probe: 33/33 checks passed` |
| `hello` | Assembly program that writes a line and exits |
| `fsclient` | Reads a file from the `initrd` and exits; `init` starts it ten times, one after the other, so the server has to release the sessions of the ones that died |
| `aborter` | Calls `abort()` so the end to end test sees its message and the process being reaped |
| `ata` | Block server for the first IDE disk: PIO reads, MBR and GPT partitions; started by `init` with the ports `0x1F0+8` and `0x3F6+1`; see [block storage](block_storage.md) |
| `fat` | FAT32 server over the block protocol; see [FAT32](fat32.md) |
| `vfs` | The file server clients talk to: mount table of `/` (`ramfs`) and `/fat` (`fat`); see [file system](filesystem.md#the-vfs-server) |
| `fatclient` | Reads files, a long name, a nested file, a multi-cluster file and a listing from `/fat`, and checks the refusals; the end to end test requires its output |
| `blkclient` | Lists the devices of `ata` and reads the first sector of the first partition; the end to end test checks its output |

The processes see each other only by name: `init` calls `cock::user::lookup("kbd")` to get a process id. The kernel names a process after its module.

## How the system starts the userland
1. The Makefile builds every `userland/*/Makefile` into `build/userland/<name>.elf`.
2. It packs the files of the `initrd/` folder and, under `bin/`, every program that is not a boot module into `build/initrd.tar` (ustar). It writes a `grub.cfg` with one `module` line for each program of `USERLAND_BOOT_ORDER` (`init`, `ramfs`, `vga_driver`), and a last line for the archive, named `initrd`. The archive is a data module (see [memory management](memory_management.md)), so it never becomes a process.
3. The kernel loads each module as a process and gives the root capabilities to the one called `init`.
4. `init` grants `ramfs` the `initrd` and sends it `CAPABILITY_GRANTED`; this is the only policy written in its code. It then reads `manifest.conf` through `ramfs`, parses it and runs each line in order (see below). A spawned process is born with no capabilities, so the grants of its line are what it gets. `vga_driver` maps the screen, and `kbd` enables the port and binds the interrupt.
5. A key press travels: keyboard controller, IRQ 1, kernel (`IRQ_NOTIFICATION`), `kbd` reads the scan code and acknowledges the line, `kbd` sends `VIDEO_WRITE_CHAR`, `vga_driver` draws the character.

A driver that needs a new capability needs a new `cap_grant` in `userland/init/main.cpp`; see [capabilities](capabilities.md). The display server is described in [VGA](vga.md) and the keyboard driver in [keyboard](keyboard.md).

## The manifest
`manifest.conf` in the initrd says which programs run and what authority each one gets. It is plain text, one entry per line; empty lines and lines that start with `#` are ignored. The parser is `shared/include/cock/abi/manifest.hpp`, which the host tests cover.

```
vga_driver caps=mem:0xA0000+0x20000
fsclient path=bin/fsclient.elf repeat=10
kbd path=bin/kbd.elf caps=io:0x60+1,irq:1 peer=vga_driver
```

| Part | Meaning |
|------|---------|
| first word | The name of the entry. It is the process name; names are unique and at most 31 bytes |
| `path=` | A file of the initrd that `init` reads and spawns. Without it the entry is a boot module that already runs, and `init` only grants it what the line says |
| `caps=` | Comma separated grants: `io:first+count` (ports), `irq:line` and `mem:first+size` (physical memory). Numbers are decimal or `0x` hexadecimal. The count of `io` and `irq` defaults to 1, `mem` needs it. At most 4 grants |
| `peer=` | Name of a process whose id travels in the `CAPABILITY_GRANTED` message (the display server for `kbd`) |
| `repeat=` | Run the program that many times (1 to 32), one after the other, waiting for each to exit. Only for entries with `path=` |

When an entry has grants or a peer, `init` sends the process `CAPABILITY_GRANTED` after granting. The kernel still checks every grant against what `init` holds, so a line cannot give more than `init` has. The parser rejects the whole file at the first bad line (unknown key, repeated key, bad number, range that wraps the address space, too many entries or grants, repeated name) and `init` then starts nothing and exits; the serial log shows the line number and the error. The file is at most 4096 bytes and 16 entries. Adding a driver means adding a line here, not editing `init`.

## The `cock::user` API
`userland/lib/include/cock/user/api.hpp` wraps the system calls for the programs written in C++:

| Function | System call |
|----------|-------------|
| `ipc_send(pid, message)`, `ipc_recv(message)` | `SYS_IPC_SEND`, `SYS_IPC_RECV` |
| `request_ports(first, count)` | `SYS_REQUEST_RESOURCE` |
| `irq_bind(line)`, `irq_ack(line)` | `SYS_IRQ_BIND`, `SYS_IRQ_ACK` |
| `cap_grant(pid, type, first, count)` | `SYS_CAP_GRANT` |
| `lookup(name)` | `SYS_PROC_LOOKUP` |
| `shm_create(pages)`, `shm_share(id, pid, writable)`, `shm_map(id, writable)`, `shm_release(id)` | `SYS_SHM_CREATE`, `SYS_SHM_SHARE`, `SYS_SHM_MAP`, `SYS_SHM_RELEASE` |
| `port_out8(port, value)`, `port_in8(port)`, `port_read_words(port, buffer, words)` | Port instructions (`outb`, `inb`, `rep insw`), allowed after `request_ports` |
| `module_open(name)` | `SYS_MODULE_OPEN` |
| `module_index(name)` | `SYS_MODULE_INDEX` |
| `spawn(image, size, name)` | `SYS_SPAWN` |
| `process_alive(pid)` | `SYS_PROC_ALIVE` |
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
* `malloc`, `calloc`, `realloc` and `free` need `stdlib/malloc.cpp`, and `operator new` and `operator delete` need `LIBCXX_SRCS = memory/user_new.cpp` (plus `string/memcpy.cpp`, `string/memset.cpp` and the `mmap` file). The allocator keeps a free list ordered by address, merges neighbouring free blocks and serves requests first fit with 8 byte alignment. It grows by anonymous `mmap` chunks of 16 pages that double up to 256 pages, or by the request rounded to pages when that is larger. Chunks are never returned, because `munmap` needs the exact region. When the process quota refuses a chunk, `malloc` returns a null pointer and the heap stays usable. `realloc` never shrinks in place and moves the block when it grows. There is no locking, so it is for single threaded programs. `operator new` calls `abort` when memory runs out, because the build has no exceptions. `abort` (`stdlib/abort.cpp`, with `unistd/_exit.cpp` and `unistd/write.cpp`) writes `abort()` to the debug output and ends the process with exit status 134 (128 plus `SIGABRT`) through `_exit`. The kernel does not build these two files: it has its own heap.
* The entry point is a function called `_start` declared `extern "C"`. It never returns: end with `exit_process`.
* `common.mk` compiles freestanding, without exceptions, RTTI, stack protector or FPU code, links at `0x400000` with `userland/linker.ld`, and tracks the headers each object includes, so changing a header rebuilds what depends on it.
* `USER_EXTRA_FLAGS` adds compiler flags for every program, for example `make iso USER_EXTRA_FLAGS=-fno-omit-frame-pointer`.
* `userland/linker.ld` produces two loadable segments: code and constants (read and execute) and data (read and write). The kernel seals the first one read only when it loads the program. A program without data has an empty second segment, which the loader accepts.

The main Makefile passes the compiler, linker and assembler (`USER_CXX`, `USER_LD`, `USER_AS`) so the userland is built with the same toolchain as the kernel. When building one program by hand, pass them yourself.

The programs run in a user-space limit of `0xC0000000` (ELF32). Their stack is the top 64 KiB below that limit, and `mmap` hands out addresses from `0x60000000` upward.

## Testing
* `bash scripts/qemu_userland_test.sh` boots the ISO under QEMU, types `abCd` on the simulated keyboard through the QEMU monitor and requires: the `hello` line and `probe: 30/30 checks passed` on the screen, the typed text on the screen, the lines of the block driver and its client (see [block storage](block_storage.md#tests); the script attaches a test disk and accepts `DISK_SCHEME=gpt`), the `printf` sample line `probe: printf -42 4294967295 FF hi Z 0xBEEF` and the `abort()` message of `aborter` on the serial log, `kbd: ready`, the VGA driver detection and the reaping of the processes that exit on the serial log, and not one exception.
* Run it also with `USER_EXTRA_FLAGS=-fno-omit-frame-pointer`, which changes how the compiler addresses locals and caught a real bug in the system call wrapper.
* Both variants run in CI, on every push.
* `bash scripts/host_test.sh` builds `host_test/` with the host compiler and runs it; it needs no emulator and checks the ring buffer, the intrusive queue, the linked list, the message layout, the capability table, the manifest parser, the partition table parsers and the FAT32 parser.
