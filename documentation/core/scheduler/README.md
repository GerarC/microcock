# MLFQ Scheduler

MicroCOCK relies on a Multi-Level Feedback Queue (MLFQ) scheduler and a software context switch driven by interrupts to achieve multitasking. The core decides what runs next; the architecture only reports that something happened (see [events](../events.md)).

The scheduler is driven by the Programmable Interval Timer (PIT), which fires `IRQ0` at a frequency of 100 Hz. Each tick calls `Scheduler::onTick`; a thread that needs to give up the CPU (it blocked, it exited, or it was hit by an exception) goes through `Scheduler::onYield`. Both end in `switchFrom` (see [switching](switching.md)).

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

## Pages of this folder

| Page | Concept |
|------|---------|
| [switching.md](switching.md) | What `switchFrom` does when it changes thread |
| [blocking_calls.md](blocking_calls.md) | How a system call blocks without sleeping in the kernel |
| [timeouts.md](timeouts.md) | The tick counter and armed threads |
| [metrics.md](metrics.md) | Response, CPU and turnaround time (`SCHED_METRICS`) |
