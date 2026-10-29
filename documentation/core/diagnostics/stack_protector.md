# Stack protector

The kernel is built with `-fstack-protector-all` (see the Makefile): the compiler puts a **canary** between the local variables and the return address of every function, and checks it before the function returns. A function that wrote past the end of a local buffer changes the canary, and the check calls `__stack_chk_fail`.

`cock/utils/ssp.hpp` holds both pieces:

* `STACK_CHK_GUARD`, the initial value of the canary, sized for the pointer width of the target (`0xE2DEE396` on i386). It is a constant, not a random value, because the kernel has no entropy source at boot.
* `__stack_chk_fail`, which the compiler generated code calls. It never returns: in the kernel it is a [panic](panic.md); in a hosted build (the host side tests) it aborts.

The canary catches a linear overflow of a local buffer before the function returns to a corrupted address. It does not catch an overflow of the whole kernel stack: that case is the guard page below each kernel stack (see [processes](../process/README.md)) and the own stack of the double fault (see [gdt](../../arch/cpu/gdt.md#fault-tasks)).
