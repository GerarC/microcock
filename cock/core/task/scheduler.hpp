#ifndef CORE_SCHEDULER_HPP
#define CORE_SCHEDULER_HPP

#include <cock/data_structure/ll.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>
#include <stddef.h>

namespace cock::core::task {
using data_structure::LinkedList;

/**
 * @brief Multi-Level Feedback Queue (MLFQ) Thread Scheduler.
 * * Manages thread execution, context switching, and priority aging/boosting.
 */
class Scheduler {
  public:
    static constexpr size_t PRIORITY_NUMBER = 5;

    /**
     * @brief Initializes the scheduling engine. Must be called before enabling interrupts.
     */
	static void init();

    /**
     * @brief Queues a thread into the appropriate ready queue based on its priority.
     * @param thread Pointer to the thread to be scheduled.
     */
	static void addThread(Thread *thread);

    /**
     * @brief Main scheduling routine. Called by the IRQ0 Timer handler to switch contexts.
     * @param current_stack_pointer The ESP of the interrupted thread.
     * @return The ESP of the next thread to execute.
     */
	static uintptr_t schedule(uintptr_t current_stack_pointer);

    /**
     * @brief Terminates the current thread and triggers a context switch.
     */
	static void exitCurrentThread(ThreadResult result);

    /**
     * @brief Voluntarily relinquishes the CPU.
     */
	static void yield();

    /**
     * @brief Retrieves the currently executing thread.
     * @return Pointer to the current Thread, or nullptr if none.
     */
	static Thread *getCurrentThread();

  private:
    static constexpr uint32_t QUANTUM = 10;
    static constexpr uint32_t BOOST_TICKS = 1000;
	static LinkedList<Thread*> readyQueues[PRIORITY_NUMBER];
	static Thread *currentThread;
    static uint32_t currentTicks;
    static uint32_t ticksSinceLastBoost;
    static bool initialized;

    /**
     * @brief Elevates all queued threads to the highest priority to prevent starvation.
     */
    static void boostPriorities();
};

} // namespace cock::core::task

#endif // !CORE_SCHEDULER_HPP
