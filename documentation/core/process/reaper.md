# Reaper (garbage collection)

The `TaskManager` keeps a global registry of all threads. When a thread terminates, it is moved to a `graveyard` queue.

A dedicated kernel thread, the **reaper**, empties the graveyard with `TaskManager::buryDeadThreads()`: it clears the registry slot, deletes the thread (and the process, if it was the last thread) and recycles the slot (see [process ids](ids.md)).

When the graveyard is empty the reaper marks itself `BLOCKED` and yields; `TaskManager::notifyThreadDeath` wakes it when the next thread dies. It runs at a high priority so the memory of a dead process comes back quickly, and it does not depend on the CPU ever being idle, as the earlier design did.

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
