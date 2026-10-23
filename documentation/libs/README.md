# Libraries

| Library | Path | What it is |
|---------|------|------------|
| C library | `libc/` | Freestanding subset: `string.h`, `stdio.h` (`printf` with buffered output, `puts`, `putchar`), `stdlib.h` (`malloc`, `abort`), `unistd.h` and `fcntl.h` file calls over the `vfs`, and the `syscall()` wrapper in `sys/syscalls.h` |
| C++ runtime | `libcxx/` | C++ ABI support (`cxa`) and `operator new` and `operator delete` over `malloc` |
| User library | `userland/lib/include/cock/user/` | `cock::user`: C++ wrappers for system calls, the clients of the console, block, file and clock servers, and the session helpers |

The kernel links none of the user-facing parts of these libraries. Rules for programs: no floating point, SSE or MMX, and the system call wrapper never addresses memory through `EBP` (see [userland](../userland/userland.md)).

Each header carries the reference of its functions as Doxygen comments; [userland/lib](../../userland/lib/README.md) lists the files of the user library.
