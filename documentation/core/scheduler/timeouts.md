# Timeouts

Timers live in the kernel because they need the timer interrupt and the scheduler; nothing a program can do replaces that, and the line of the timer is reserved. `Timeouts` (`cock/core/task/timeouts.*`) owns the tick counter and a list of armed threads, linked through `Thread::timerLink`, so arming never allocates. Each thread has a `TimeoutState` (`NONE`, `ARMED`, `EXPIRED`) and a `wakeTick`.

* `Scheduler::onTick` calls `Timeouts::onTick` first: the counter grows, every armed thread whose `wakeTick` has passed becomes `EXPIRED`, and if it was `WAITING_MSG` or `BLOCKED` it is made `READY` and queued. The scan is linear in the number of armed threads.
* `Timeouts::wait(thread, milliseconds, state)` is the blocking step of `SYS_SLEEP` and `SYS_IPC_RECV_TIMEOUT`: with the state `EXPIRED` it clears it and reports `TIMED_OUT`; with `NONE` it arms the thread (a time of zero reports `TIMED_OUT` at once) and blocks it with a restart (see [blocking calls](blocking_calls.md)). A thread already `ARMED` blocks again without changing its deadline.
* The deadline is `now + ceil(ms * 100 / 1000) + 1` ticks. The extra tick covers the partial tick in which the call was made, so a wait lasts at least the time asked and less than one tick more.
* `Timeouts::cancel` removes a thread from the list and clears the state. A receive that finds a message calls it, and `~Thread` calls it, so a dead thread never stays in the list.
