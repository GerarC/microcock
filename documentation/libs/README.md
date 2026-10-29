# Libraries

| Library | Path | What it is |
|---------|------|------------|
| C library | `libc/` | Freestanding subset: `string.h`, `stdio.h` (`printf`, `puts` and `putchar` over a buffer of standard output, see [descriptors](descriptors.md)), `stdlib.h` (`malloc`, `abort`), `unistd.h` and `fcntl.h` file calls over the `vfs`, and the `syscall()` wrapper in `sys/syscalls.h` |
| C++ runtime | `libcxx/` | C++ ABI support (`cxa`) and `operator new` and `operator delete` over `malloc` |
| User library | `userland/lib/include/cock/user/` | `cock::user`: C++ wrappers for system calls, the clients of the console, block, file and clock servers, and the session helpers |

The kernel links none of the user-facing parts of these libraries. Rules for programs: no floating point, SSE or MMX, and the system call wrapper never addresses memory through `EBP` (see [syscall ABI](syscall_abi.md)).

How a program's descriptors work, and what `dup2` can redirect, is in [descriptors](descriptors.md).

Each header carries the reference of its functions as Doxygen comments; [userland/lib](../../userland/lib/README.md) lists the files of the user library.

## Pages

| Page | Topic |
|------|-------|
| [user_api.md](user_api.md) | The `cock::user` wrappers of the system calls and the helpers built on them |
| [syscall_abi.md](syscall_abi.md) | Registers of `int 0x80` as a program sees them, the wrapper and blocking calls |
| [descriptors.md](descriptors.md) | Descriptors in the C library: `dup`, `dup2`, `fork`, `exec`, buffered output |
