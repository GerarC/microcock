# Task Management & Scheduling

MicroCOCK relies on a Multi-Level Feedback Queue (MLFQ) scheduler and a software context switch driven by interrupts to achieve multitasking. The core decides what runs next; the architecture only reports that something happened.

## Processes and threads

A **`Process`** (`cock/core/task/process.hpp`) owns everything that threads of the same program share:

* the address space and the user-space limit of its ELF class,
* the capabilities ([capabilities](capabilities.md)),
* the architecture context (`ArchProcessContext`), stored **by value**: on x86 it is the I/O permission bitmap and a version number,
* a name (the Multiboot module name),
* the `mmap` cursor and the list of mapped regions.

A **`Thread`** (`cock/core/task/thread.hpp`) is an execution context inside a process. Threads can be `KERNEL` (Ring 0) or `USER` (Ring 3).

* Every thread has a **16 KiB kernel stack** with an unmapped guard page below it.
* User threads also get a **64 KiB user stack** at the top of the user-space limit of their process.
* The kernel has its own process, which owns the kernel threads (the idle thread among them).
* A user process has exactly **one thread** for now. The process id is the id of that first thread, so it is also the address used by IPC.

When the last thread of a user process is destroyed, the process is destroyed with it: its address space, the page tables and every owned frame go back to the PMM, and the IRQ lines it had bound are released.

## The MLFQ Scheduler
The scheduler is driven by the Programmable Interval Timer (PIT), which fires `IRQ0` at a frequency of 100 Hz. Each tick calls `Scheduler::onTick`; a thread that needs to give up the CPU (it blocked, it exited, or it was hit by an exception) goes through `Scheduler::onYield`. Both end in `switchFrom`.

The scheduler maintains 5 ready queues corresponding to thread priorities, ranging from `IDLE` to `CRITICAL`. The queues are intrusive: the links live inside `Thread`, so queueing a thread never allocates.

* **Quantum:** a thread keeps the CPU for 10 ticks.
* **Aging:** If a thread consumes its entire time quantum without yielding, its priority is demoted.
* **Priority Boosting:** To prevent starvation, all queued threads are boosted to the highest priority queue every 1000 ticks.
* **Idle thread:** it never enters the queues. It runs only when nothing else is ready.

```cpp
void Scheduler::boostPriorities() {
	size_t max_index = PRIORITY_NUMBER - 1;
	ThreadPriority max_priority = static_cast<ThreadPriority>(max_index);
	for (size_t p = 0; p < max_index; p++) {
		Thread *thread = nullptr;
		while ((thread = readyQueues[p].pop()) != nullptr) {
			thread->setPriority(max_priority);
			readyQueues[max_index].push(thread);
		}
	}
}
```

### Switching
`switchFrom` saves the stack pointer of the thread that stops, picks the next one, and then:

1. tells the architecture the kernel stack to use on the next entry from Ring 3,
2. applies the process context only if the next thread belongs to a user process,
3. changes the address space only if it differs from the current one.

### Blocking system calls
A system call cannot sleep in the middle of a handler, because the kernel does not run nested interrupts. Instead, a handler that cannot finish calls `Scheduler::requestReschedule()` and `Scheduler::requestRestart()` and returns. The core then rewinds the saved instruction pointer by the size of the `int 0x80` instruction and switches to another thread. When the thread runs again, it executes the system call again from the start, with the same registers. `sys_ipc_recv` and the output path of `sys_write` work this way.

## Events
`cock/core/events.cpp` is the single place where the architecture hands control to the core:

| Event | What the core does |
|-------|--------------------|
| System call | Runs the handler, then applies a pending restart and reschedule |
| Exception from Ring 3 | Logs it, terminates the thread, and reschedules |
| Exception from Ring 0 | Panics |
| Timer interrupt | `Scheduler::onTick` |
| Any other IRQ | Forwards it to the process that bound the line, if any |

## Garbage Collection
The `TaskManager` keeps a global registry of all threads. When a thread terminates, it is moved to a `graveyard` queue. The Kernel's idle thread periodically invokes `TaskManager::buryDeadThreads()`, which clears the registry slot, deletes the thread (and the process, if it was the last thread) and recycles the id.

## Context of the architecture
`ArchProcessContext` holds the I/O permission bitmap of the process. Copying a bitmap into the TSS at every switch would be the most expensive thing the kernel did, so it is lazy:

* each context has a **version** that changes when its permissions change,
* the TSS remembers the version it holds, and copies nothing when it already matches,
* only the bytes in use are copied, and a process with no ports turns the bitmap off instead of copying anything,
* kernel threads never reload it.
