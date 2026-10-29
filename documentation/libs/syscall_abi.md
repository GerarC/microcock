# System call ABI (x86)

| Register | Content |
|----------|---------|
| `EAX` | System call number on entry, result on return (`-1` is failure) |
| `EBX`, `ECX`, `EDX`, `ESI`, `EDI` | Arguments 1 to 5 |
| `EBP` | Argument 6 |

All the other registers are preserved. `libc/include/sys/syscalls.h` has the inline `syscall()` that does the call.

Programs cannot use floating point, MMX or SSE: the kernel sets `CR0.EM`, so those instructions fault and the kernel terminates the process. The programs are compiled with `-mno-sse -mno-mmx -mno-80387` so the compiler never emits them.

## Why the wrapper looks the way it does
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

## Blocking calls and `int 0x80`
A call that has to wait is restarted: the kernel rewinds the instruction pointer to the `int $0x80` and runs the call again when the thread wakes up, with the registers as they were. The wrapper does not notice, except that the call takes longer.
