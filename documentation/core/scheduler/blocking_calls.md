# Blocking system calls

A system call cannot sleep in the middle of a handler, because the kernel does not run nested interrupts. Instead, a handler that cannot finish calls `Scheduler::requestReschedule()` and `Scheduler::requestRestart()` and returns. The core then rewinds the saved instruction pointer by the size of the `int 0x80` instruction and switches to another thread. When the thread runs again, it executes the system call again from the start, with the same registers. `sys_ipc_recv` and the output path of `sys_write` work this way.
