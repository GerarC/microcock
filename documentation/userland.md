# Userland

Everything that is not the kernel runs in Ring 3 as a program loaded from a Multiboot module. This page describes the programs that ship with the system, how they are built and started, the system call ABI, and how to write a new one.

## Programs

| Program | Role |
|---------|------|
| `init` | Root server. The only process born with authority; it gives each driver its capabilities and tells it to start, then exits |
| `vga_driver` | Display server. Maps the video memory and draws what it receives by IPC |
| `kbd` | PS/2 keyboard driver. Binds IRQ 1, reads port `0x60`, decodes the scan codes and sends characters to the display server |
| `probe` | Security probe. Checks from Ring 3 that the kernel denies what it must, and prints `probe: 16/16 checks passed` |
| `hello` | Assembly program that writes a line and exits |

The processes see each other only by name: `init` calls `cock::user::lookup("kbd")` to get a process id. The kernel names a process after its module.

## How the system starts the userland
1. The Makefile builds every `userland/*/Makefile` into `build/userland/<name>.elf`.
2. It writes a `grub.cfg` with one `module` line per program, named after the program, with `init` first (`USERLAND_BOOT_ORDER` in the Makefile).
3. The kernel loads each module as a process and gives the root capabilities to the one called `init`.
4. `init` grants the capabilities and sends `CAPABILITY_GRANTED` to the drivers. `vga_driver` then maps the screen, and `kbd` enables the port and binds the interrupt.
5. A key press travels: keyboard controller, IRQ 1, kernel (`IRQ_NOTIFICATION`), `kbd` reads the scan code and acknowledges the line, `kbd` sends `VIDEO_WRITE_CHAR`, `vga_driver` draws the character.

A driver that needs a new capability needs a new `cap_grant` in `userland/init/main.cpp`; see [capabilities](capabilities.md). The display server is described in [VGA](vga.md) and the keyboard driver in [keyboard](keyboard.md).

## The `cock::user` API
`userland/lib/include/cock/user/api.hpp` wraps the system calls for the programs written in C++:

| Function | System call |
|----------|-------------|
| `ipc_send(pid, message)`, `ipc_recv(message)` | `SYS_IPC_SEND`, `SYS_IPC_RECV` |
| `request_ports(first, count)` | `SYS_REQUEST_RESOURCE` |
| `irq_bind(line)`, `irq_ack(line)` | `SYS_IRQ_BIND`, `SYS_IRQ_ACK` |
| `cap_grant(pid, type, first, count)` | `SYS_CAP_GRANT` |
| `lookup(name)` | `SYS_PROC_LOOKUP` |
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
* The entry point is a function called `_start` declared `extern "C"`. It never returns: end with `exit_process`.
* `common.mk` compiles freestanding, without exceptions, RTTI, stack protector or FPU code, links at `0x400000` with `userland/linker.ld`, and tracks the headers each object includes, so changing a header rebuilds what depends on it.
* `USER_EXTRA_FLAGS` adds compiler flags for every program, for example `make iso USER_EXTRA_FLAGS=-fno-omit-frame-pointer`.
* `userland/linker.ld` produces two loadable segments: code and constants (read and execute) and data (read and write). The kernel seals the first one read only when it loads the program. A program without data has an empty second segment, which the loader accepts.

The main Makefile passes the compiler, linker and assembler (`USER_CXX`, `USER_LD`, `USER_AS`) so the userland is built with the same toolchain as the kernel. When building one program by hand, pass them yourself.

The programs run in a user-space limit of `0xC0000000` (ELF32). Their stack is the top 64 KiB below that limit, and `mmap` hands out addresses from `0x60000000` upward.

## Testing
* `bash scripts/qemu_userland_test.sh` boots the ISO under QEMU, types `abCd` on the simulated keyboard through the QEMU monitor and requires: the `hello` line and `probe: 16/16 checks passed` on the screen, the typed text on the screen, `kbd: ready`, the VGA driver detection and the reaping of the processes that exit on the serial log, and not one exception.
* Run it also with `USER_EXTRA_FLAGS=-fno-omit-frame-pointer`, which changes how the compiler addresses locals and caught a real bug in the system call wrapper.
* Both variants run in CI, on every push.
* `bash scripts/host_test.sh` builds `host_test/` with the host compiler and runs it; it needs no emulator and checks the ring buffer, the intrusive queue, the linked list, the message layout and the capability table.
