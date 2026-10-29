# Threads

The kernel provides only the mechanism: a thread, a stack, and a way to block. Everything else (a thread library, a mutex, a join) is built by the program. The thread object and its place in a process are described in [processes](../process/README.md).

* `SYS_THREAD_CREATE(entry, stack_base, stack_size)` starts a thread of the caller process. The **program owns the stack**: the kernel checks that `entry` is a user address and that the stack is a writable user range of at least 8 KiB, and it never maps or frees user stacks. The thread starts at `entry` with `ESP` at the top of that range minus 4. The call fails, without panicking, when the process already has 8 threads (dead threads count until the [reaper](../process/reaper.md) has freed them), when there is no free id or when kernel stack memory is out of reach. `userland/lib/include/cock/user/thread.hpp` (`thread_start`) writes an argument and a return address that ends the thread at the top of the stack, so a thread function is `void f(void *)` and may simply return.
* `SYS_EXIT` ends the calling thread. The process ends when its last thread ends, and its exit status is the one given by that last thread. An exception ends only the faulting thread, with the status `FAULT_STATUS`; a program that cannot survive that must not share state between threads carelessly.
* `SYS_GETTID` returns the id of the calling thread (`SYS_GETPID` returns the process id).

A thread can also be `WAITING_CALL` or `WAITING_REPLY` (see [synchronous calls](../ipc.md#synchronous-calls)).

## Blocking and the user library

The blocking primitive is the [futex](futex.md). The user library builds the rest on it, with no kernel change:

* `userland/lib/include/cock/user/mutex.hpp` is a mutex over the futex (three states: free, locked, locked with waiters).
* `condition_variable.hpp` (`ConditionVariable`: `wait`, `signal`, `broadcast`) and `semaphore.hpp` (`Semaphore`: `wait`, `tryWait`, `post`) are built on the same two calls. The condition variable keeps a wake counter as its futex word and reads it **before** it releases the mutex, so a signal that arrives before the thread sleeps changes the word and `futex_wait` returns at once; callers wait in a loop on their own condition, because a thread may return without a signal. The semaphore calls `futex_wake` only when a thread sleeps in `wait`.
* `joinable_thread.hpp` (`JoinableThread`: `start`, `join`) adds a join with no kernel change: the thread function runs inside a wrapper that sets a state word and wakes the joiner, and `join` then waits for `process_alive` of the thread id to turn false, so the caller may reuse the stack as soon as `join` returns. `join` takes an optional time limit. A thread function that ends with `exit_process` instead of returning is never reported as finished.
