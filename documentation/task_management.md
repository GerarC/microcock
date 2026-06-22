# Task Management & Scheduling

MicroCOCK relies on a Multi-Level Feedback Queue (MLFQ) scheduler and a hardware-assisted context switching mechanism to achieve multitasking.

## Thread Representation
Execution contexts are represented by the `Thread` class. Threads can be initialized as `KERNEL` (Ring 0) or `USER` (Ring 3) threads. User threads are assigned a dedicated 64KB stack and an isolated virtual address space.

To prevent memory leaks when a thread terminates or crashes, the Thread object maintains a `LinkedList<MemoryRegion>` tracking all dynamically mapped pages.

## The MLFQ Scheduler
The scheduler is driven by the Programmable Interval Timer (PIT), which fires `IRQ0` at a frequency of 100 Hz. Upon receiving the interrupt, `Scheduler::schedule` evaluates the currently running thread.

The scheduler maintains 5 ready queues corresponding to thread priorities, ranging from `IDLE` to `CRITICAL`.
* **Aging:** If a thread consumes its entire time quantum without yielding, its priority is demoted.
* **Priority Boosting:** To prevent starvation, all queued threads are periodically boosted to the highest priority queue.

```cpp
void Scheduler::boostPriorities() {
    ThreadPriority max_priority = static_cast<ThreadPriority>(PRIORITY_NUMBER - 1);
    for (size_t p = 0; p < PRIORITY_NUMBER - 1; p++) { 
        Thread *thread = nullptr;
        while (readyQueues[p].popFront(thread)) {
            thread->setPriority(max_priority);
            readyQueues[PRIORITY_NUMBER - 1].append(thread); 
        }
    }
    Logger::trace("[MLFQ] Priority Boost: All threads moved to CRITICAL"); 
}
```

## Garbage Collection
The `TaskManager` keeps a global registry of all threads. When a thread terminates, its ID is moved to a `graveyard` list. The Kernel's idle thread periodically invokes `TaskManager::buryDeadThreads()` to free the dead thread objects and recycle their Process IDs.
