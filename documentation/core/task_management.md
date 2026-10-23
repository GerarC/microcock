# Task Management & Scheduling

MicroCOCK relies on a Multi-Level Feedback Queue (MLFQ) scheduler and a software context switch driven by interrupts to achieve multitasking. The core decides what runs next; the architecture only reports that something happened.

## Processes and threads

A **`Process`** (`cock/core/task/process.hpp`) owns everything that threads of the same program share:

* the address space and the user-space limit of its ELF class,
* the capabilities ([capabilities](capabilities.md)),
* the architecture context (`ArchProcessContext`), stored **by value**: on x86 it is a pointer to the I/O permission bitmap (null until the process is granted a port, so a process without ports costs nothing) and a version number,
* a name (the Multiboot module name),
* a **fixed table of mapping regions** (up to 32) and the count of pages it owns (up to 4096). Anonymous memory counts against that limit, device memory does not. See [memory management](memory_management.md#per-process-limits).

A **`Thread`** (`cock/core/task/thread.hpp`) is an execution context inside a process. Threads can be `KERNEL` (Ring 0) or `USER` (Ring 3).

* Every thread has a **16 KiB kernel stack** with an unmapped guard page below it.
* User threads also get a **64 KiB user stack** at the top of the user-space limit of their process.
* The kernel has its own process, which owns the kernel threads (the idle thread among them).
* A user process has from 1 to 8 threads (`MAX_THREADS_PER_PROCESS`). The process id is the id of its **first** thread, so it is also the address used by IPC and by `SYS_PROC_ALIVE`; every other thread has an id and an inbox of its own, and a message carries the id of the thread that sent it. Once the first thread ends, the process id no longer receives messages even if other threads run.
* Each thread carries, besides its inbox, a mask of **pending interrupt notifications** (see [ipc](ipc.md#interrupts-as-messages)).

When the last thread of a user process is destroyed, the process is destroyed with it: its address space, the page tables and every owned frame go back to the PMM, and the IRQ lines it had bound are released.

## Spawn
`SYS_SPAWN(image, size, name, name_length)` starts a process from an ELF image in the memory of the caller. Only a process that holds a `SPAWN` capability may call it (see [capabilities](capabilities.md#the-spawn-capability)). The kernel validates both ranges, bounds the image to 4 MiB, copies it into kernel pages (the loader switches address spaces while it copies, so it cannot read user memory), builds the address space with the same loader as the boot modules, names the process and starts its single thread. The new process has an **empty capability set**; whoever spawned it grants what it needs with `SYS_CAP_GRANT` and `CAPABILITY_GRANTED`. The call fails, without panicking, when the image is not a loadable ELF, when there are no free process ids or when kernel pages run out; the kernel can still panic if the new process cannot get its stack. `SYS_PROC_ALIVE(pid)` tells whether a process id names a living process.

## The MLFQ Scheduler
The scheduler is driven by the Programmable Interval Timer (PIT), which fires `IRQ0` at a frequency of 100 Hz. Each tick calls `Scheduler::onTick`; a thread that needs to give up the CPU (it blocked, it exited, or it was hit by an exception) goes through `Scheduler::onYield`. Both end in `switchFrom`.

The scheduler maintains 5 ready queues corresponding to thread priorities, ranging from `IDLE` to `CRITICAL`. The queues are intrusive: the links live inside `Thread`, so queueing a thread never allocates.

* **Quantum:** a thread keeps the CPU for 10 ticks.
* **Aging:** the time a thread runs is charged to its current level (`Thread::levelTicks`). When the sum reaches the allowance (10 ticks, the size of a quantum), the thread drops one level the next time it stops, whether it was preempted, yielded or blocked. Yielding early does not reset the count, so a thread cannot keep a high level by giving up the CPU before the quantum ends. A new level, by demotion or by the boost, starts the count at zero.
* **How the time is measured:** a timer tick that fires while a thread runs charges it one whole tick. A thread that stops between two ticks is charged the cycles it ran, read from the time stamp counter (`hal::read_cycle_counter`) at the dispatch and at the switch, and the cycles add up to whole ticks (`Thread::chargeLevelCycles`). The tick period in cycles is measured from the gap between two timer ticks (`Scheduler::measureTick`), so the rate of the counter need not be known; gaps under 10000 cycles are not taken as a measurement, and until one exists only whole ticks are charged. A thread that runs a few microseconds and blocks pays almost nothing, so an I/O bound thread keeps its level. A run is never charged more than one tick, because a tick ends it. This assumes a time stamp counter that runs at a constant rate, which the emulator and recent CPUs provide.
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

### Metrics
A kernel built with `SCHED_METRICS=1` keeps three figures per thread, in timer ticks (10 ms):

* **Response time:** from the moment the thread is first queued as ready until it first runs.
* **CPU time:** the ticks that fired while the thread was running, so it is rounded to ticks.
* **Turnaround time:** from the creation of the thread until it is read, or until it dies.

The hooks are in `SchedMetrics` (`sched_metrics.hpp`): `onCreate` in `Thread::initBase`, `onReady` in `Scheduler::enqueue`, `onTick` in `Scheduler::onTick`, `onDispatch` in `switchFrom` and `onExit` in `TaskManager::notifyThreadDeath`, which logs the three figures at trace level. Each hook is a template whose body sits behind `if constexpr (SCHED_METRICS_ENABLED)`, and `Thread::metrics` is an empty `[[no_unique_address]]` struct without the flag. Without it there is no extra code and no extra byte in a thread, and `SYS_SCHED_METRICS` is not compiled or registered. With it, a thread reads its own figures into an `abi::sched::Metrics` (`cock/abi/sched_metrics.hpp`) through `cock::user::sched_metrics`. The test program `schedbench` runs a CPU bound and an I/O bound thread and prints both sets; the I/O bound thread must use less CPU time.

### Switching
`switchFrom` saves the stack pointer of the thread that stops, picks the next one, and then:

1. tells the architecture the kernel stack to use on the next entry from Ring 3,
2. applies the process context only if the next thread belongs to a user process,
3. changes the address space only if it differs from the current one.

### Blocking system calls
A system call cannot sleep in the middle of a handler, because the kernel does not run nested interrupts. Instead, a handler that cannot finish calls `Scheduler::requestReschedule()` and `Scheduler::requestRestart()` and returns. The core then rewinds the saved instruction pointer by the size of the `int 0x80` instruction and switches to another thread. When the thread runs again, it executes the system call again from the start, with the same registers. `sys_ipc_recv` and the output path of `sys_write` work this way.

## Threads
The kernel provides only the mechanism: a thread, a stack, and a way to block. Everything else (a thread library, a mutex, a join) is built by the program.

* `SYS_THREAD_CREATE(entry, stack_base, stack_size)` starts a thread of the caller process. The **program owns the stack**: the kernel checks that `entry` is a user address and that the stack is a writable user range of at least 8 KiB, and it never maps or frees user stacks. The thread starts at `entry` with `ESP` at the top of that range minus 4. The call fails, without panicking, when the process already has 8 threads (dead threads count until the reaper has freed them), when there is no free id or when kernel stack memory is out of reach. `userland/lib/include/cock/user/thread.hpp` (`thread_start`) writes an argument and a return address that ends the thread at the top of the stack, so a thread function is `void f(void *)` and may simply return.
* `SYS_EXIT` ends the calling thread. The process ends when its last thread ends, and its exit status is the one given by that last thread. An exception ends only the faulting thread, with the status `FAULT_STATUS`; a program that cannot survive that must not share state between threads carelessly.
* `SYS_GETTID` returns the id of the calling thread (`SYS_GETPID` returns the process id).
* `SYS_FUTEX_WAIT(word, expected, milliseconds)` and `SYS_FUTEX_WAKE(word, count)` are the blocking primitive. `word` must be a 4 byte aligned, readable user address. The wait compares the word with `expected` with interrupts off: if it differs it returns `1` at once; if it is equal the thread joins the wait list (`Futex`, a FIFO keyed by process and address) and blocks. It returns `0` when woken and `2` when the time ran out (`0xFFFFFFFF` means no limit, `0` never waits). The wake takes up to `count` waiters of the **same process** on that address and returns how many it woke; a wake also cancels the timeout of each. The restart pattern of blocking calls applies: a thread that wakes runs the call again and finds the wake mark on the thread, not the word, so the word does not have to change. `~Thread` removes a dead waiter from the list.
* `userland/lib/include/cock/user/mutex.hpp` is a mutex over the futex (three states: free, locked, locked with waiters).
* `condition_variable.hpp` (`ConditionVariable`: `wait`, `signal`, `broadcast`) and `semaphore.hpp` (`Semaphore`: `wait`, `tryWait`, `post`) are built on the same two calls. The condition variable keeps a wake counter as its futex word and reads it **before** it releases the mutex, so a signal that arrives before the thread sleeps changes the word and `futex_wait` returns at once; callers wait in a loop on their own condition, because a thread may return without a signal. The semaphore calls `futex_wake` only when a thread sleeps in `wait`.
* `joinable_thread.hpp` (`JoinableThread`: `start`, `join`) adds a join with no kernel change: the thread function runs inside a wrapper that sets a state word and wakes the joiner, and `join` then waits for `process_alive` of the thread id to turn false, so the caller may reuse the stack as soon as `join` returns. `join` takes an optional time limit. A thread function that ends with `exit_process` instead of returning is never reported as finished.

A thread can also be `WAITING_CALL` or `WAITING_REPLY` (see [synchronous calls](ipc.md#synchronous-calls)); `Scheduler::requestHandoff` lets a system call name the thread that runs next, which `switchFrom` honours once, before it looks at the ready queues.

## Exit notifications
`SYS_SPAWN` takes a flags argument; `SPAWN_NOTIFY_EXIT` asks the kernel to tell the spawner when the new process ends. The process keeps its parent id, the flag and an exit status. When its last thread ends, `TaskManager::notifyThreadDeath` stores the notice in the record of the parent process (`IPC::notifyExit`), and the main thread of the parent receives it as a `PROCESS_EXITED` message from `KERNEL_SENDER` (`arg0` the process id, `arg1` the status), before any message of the inbox. The record is not the inbox, so a full inbox cannot lose it. It never overflows: `SYS_SPAWN` with the flag reserves one of the `MAX_NOTIFIED_CHILDREN` (16) slots of the spawner and fails when none is free; the slot is given back when the parent reads the notice. A spawner that does not read therefore stops being able to start more notified children, instead of losing a notice. Only the main thread of the parent reads notices; if the parent process ends first, its record goes with it. `SYS_PROC_ALIVE` remains a check that needs no notice. `init` asks for it for the programs the manifest repeats (`repeat=`) and waits for the notice with a receive timeout; it prints `init: <name> exit notices N/M` when the run is over. Restarting a server that died is not done yet: the notice gives `init` what it needs, the policy is still to be written.

## Timeouts
Timers live in the kernel because they need the timer interrupt and the scheduler; nothing a program can do replaces that, and the line of the timer is reserved. `Timeouts` (`cock/core/task/timeouts.*`) owns the tick counter and a list of armed threads, linked through `Thread::timerLink`, so arming never allocates. Each thread has a `TimeoutState` (`NONE`, `ARMED`, `EXPIRED`) and a `wakeTick`.

* `Scheduler::onTick` calls `Timeouts::onTick` first: the counter grows, every armed thread whose `wakeTick` has passed becomes `EXPIRED`, and if it was `WAITING_MSG` or `BLOCKED` it is made `READY` and queued. The scan is linear in the number of armed threads.
* `Timeouts::wait(thread, milliseconds, state)` is the blocking step of `SYS_SLEEP` and `SYS_IPC_RECV_TIMEOUT`: with the state `EXPIRED` it clears it and reports `TIMED_OUT`; with `NONE` it arms the thread (a time of zero reports `TIMED_OUT` at once) and blocks it with a restart. A thread already `ARMED` blocks again without changing its deadline.
* The deadline is `now + ceil(ms * 100 / 1000) + 1` ticks. The extra tick covers the partial tick in which the call was made, so a wait lasts at least the time asked and less than one tick more.
* `Timeouts::cancel` removes a thread from the list and clears the state. A receive that finds a message calls it, and `~Thread` calls it, so a dead thread never stays in the list.

## Events
`cock/core/events.cpp` is the single place where the architecture hands control to the core:

| Event | What the core does |
|-------|--------------------|
| System call | Runs the handler, then applies a pending restart and reschedule |
| Exception from Ring 3 | Logs it, terminates the thread, and reschedules |
| Exception from Ring 0 | Panics |
| Timer interrupt | `Scheduler::onTick` |
| Any other IRQ | Masks the line and raises a pending notification on the thread that bound it, waking it if it waits |

## Garbage Collection
The `TaskManager` keeps a global registry of all threads. When a thread terminates, it is moved to a `graveyard` queue.

A dedicated kernel thread, the **reaper**, empties the graveyard with `TaskManager::buryDeadThreads()`: it clears the registry slot, deletes the thread (and the process, if it was the last thread) and recycles the slot.

## Process ids
An id is `generation << 12 | slot`: the low 12 bits index the registry (4096 slots) and the high 19 bits count how many times that slot has been reused, so every id stays below 2^31. The first user of a slot has generation 0, which is why the first ids are small numbers. When the reaper recycles a slot it increments the generation, so an id kept by `init`, a driver or the kernel (`debug_output_pid`) stops matching: `TaskManager::getThreadByPID` compares the whole id, and a message to a dead process fails instead of reaching the thread that took its slot. After 2^19 reuses of one slot the generation wraps. When the graveyard is empty the reaper marks itself `BLOCKED` and yields; `TaskManager::notifyThreadDeath` wakes it when the next thread dies. It runs at a high priority so the memory of a dead process comes back quickly, and it does not depend on the CPU ever being idle, as the earlier design did.

```cpp
ThreadResult kernel_reaper() {
	while (true) {
		TaskManager::buryDeadThreads();
		{
			core::hal::InterruptGuard guard;
			if (TaskManager::graveyardEmpty())
				Scheduler::getCurrentThread()->setState(ThreadState::BLOCKED);
		}
		Scheduler::yield();
	}
	return ThreadResult::success();
}
```

The check and the state change happen under the interrupt guard, and a thread can only die from a context that runs with interrupts off, so a death cannot slip in between and be missed. The reaper is created after the boot modules, so their process ids do not change. The idle thread now only halts the CPU.

## Pools
`Thread` and `Process` have their own `operator new` and `operator delete` over a `SlabPool` (`cock/core/memory/slab_pool.hpp`): chunks of four pages come from `VirtualMemoryManager::allocPages`, are cut into slots and kept in a free list, and a released slot is the next one handed out. Neither class touches the first-fit heap, a thread that dies leaves its slot to the next one, and the kernel virtual range stays flat while threads and processes come and go. Chunks are never returned. The I/O permission bitmap (8 KiB) is allocated from the heap when a process asks for its first port, and freed with the process; a request that cannot get the memory is denied.

## Context of the architecture
`ArchProcessContext` holds the I/O permission bitmap of the process. Copying a bitmap into the TSS at every switch would be the most expensive thing the kernel did, so it is lazy:

* each context has a **version** that changes when its permissions change,
* the TSS remembers the version it holds, and copies nothing when it already matches,
* only the bytes in use are copied, and a process with no ports turns the bitmap off instead of copying anything,
* kernel threads never reload it.
