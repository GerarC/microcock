# Switching

`switchFrom` saves the stack pointer of the thread that stops, picks the next one, and then:

1. tells the architecture the kernel stack to use on the next entry from Ring 3,
2. applies the process context only if the next thread belongs to a user process (see [arch context](../process/arch_context.md)),
3. changes the address space only if it differs from the current one.

A system call can name the thread that runs next with `Scheduler::requestHandoff`, which `switchFrom` honours once, before it looks at the ready queues (see [synchronous calls](../ipc.md#synchronous-calls)).
