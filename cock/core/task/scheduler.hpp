/** @file
 * @brief The thread scheduler: a multi level feedback queue driven by the
 * timer tick and by explicit requests from system calls.
 */
#ifndef CORE_SCHEDULER_HPP
#define CORE_SCHEDULER_HPP

#include <cock/core/task/thread.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

/**
 * @brief Multi-Level Feedback Queue (MLFQ) Thread Scheduler.
 * * Manages thread execution, context switching, and priority aging/boosting.
 *
 * All state is static and the scheduler is single CPU. Switching is done by
 * returning the saved context of the next thread to the HAL, which resumes it.
 * Syscall handlers do not switch directly: they raise a reschedule or a
 * restart request that the dispatcher honours when the handler returns.
 */
class Scheduler {
  public:
	/** @brief Number of priority levels, the highest value runs first. */
	static constexpr size_t PRIORITY_NUMBER = 5;

    /**
     * @brief Initializes the scheduling engine. Must be called before enabling interrupts.
     */
	static void init();
	/**
	 * @brief Sets the thread that runs when nothing else is ready.
	 * @param thread Idle thread. It is never queued.
	 */
	static void setIdleThread(Thread *thread);
	/**
	 * @brief Makes a thread runnable.
	 * @param thread Thread to enqueue at its priority. Null, the idle thread
	 * and a thread already queued are ignored.
	 * @note Blocks interrupts while it runs.
	 */
	static void addThread(Thread *thread);
	/**
	 * @brief Timer interrupt entry: advances timeouts, boosts priorities and
	 * preempts when the quantum is over.
	 * @param current_stack_pointer Saved context of the interrupted thread.
	 * @return Saved context to resume. It is the same when no switch happens or
	 * the scheduler is not initialized.
	 * @note Interrupt context. A thread that used its whole quantum loses one
	 * priority level.
	 */
	static uintptr_t onTick(uintptr_t current_stack_pointer);
	/**
	 * @brief Switches to the next thread after a yield, a block or a death.
	 * @param current_stack_pointer Saved context of the current thread.
	 * @return Saved context of the next thread, with its address space and
	 * kernel stack already installed. Panics when there is nothing to run and
	 * no idle thread.
	 */
	static uintptr_t onYield(uintptr_t current_stack_pointer);
	/**
	 * @brief Marks the current thread dead and requests a reschedule.
	 * @note Notifies the task manager. Panics if the idle thread is asked to
	 * terminate. Does nothing without a live current thread.
	 */
	static void terminateCurrentThread();

	/** @brief Asks for a thread switch when the current system call returns. */
	static void requestReschedule();
	/**
	 * @brief Reads and clears the reschedule request.
	 * @return True when one was pending.
	 */
	static bool takeRescheduleRequest();

	/**
	 * @brief Asks that the next switch goes to a given thread.
	 * @param thread Thread to run next. It is used only if it is ready and not
	 * queued, otherwise normal selection applies.
	 */
	static void requestHandoff(Thread *thread);
	/**
	 * @brief Records where the registers of the current system call are saved.
	 * @param frame Saved context of the calling thread.
	 */
	static void setSyscallFrame(uintptr_t frame) { syscallFrame = frame; }
	/** @return The saved context of the current system call. */
	static uintptr_t getSyscallFrame() { return syscallFrame; }

	/**
	 * @brief Asks that the current system call is executed again when the
	 * thread resumes. Used by every blocking call.
	 */
	static void requestRestart();
	/**
	 * @brief Reads and clears the restart request.
	 * @return True when one was pending.
	 */
	static bool takeRestartRequest();
	/**
	 * @brief Ends the current kernel thread with a result.
	 * @param result Outcome of the thread function. An error is logged.
	 * @note Terminates the thread and yields, so it does not return.
	 */
	static void exitCurrentThread(ThreadResult result);

    /**
     * @brief Voluntarily relinquishes the CPU.
     * @note A running current thread is made ready first, then the HAL yield
     * is invoked. Process context only.
     */
	static void yield();

    /**
     * @brief Retrieves the currently executing thread.
     * @return Pointer to the current Thread, or nullptr if none.
     */
	static Thread *getCurrentThread();

  private:
	/** @brief Timer ticks a thread may run before being preempted. */
	static constexpr uint32_t QUANTUM = 10;
	/** @brief Ticks between priority boosts that prevent starvation. */
	static constexpr uint32_t BOOST_TICKS = 1000;
	static ThreadQueue readyQueues[PRIORITY_NUMBER]; /**< One queue per priority. */
	static Thread *currentThread; /**< Thread running now. */
	static Thread *idleThread; /**< Thread run when nothing is ready. */
	static uint32_t currentTicks; /**< Ticks used by the current thread. */
	static uint32_t ticksSinceLastBoost; /**< Ticks since the last boost. */
	static bool initialized; /**< True after init. */
	static bool rescheduleRequested; /**< Pending reschedule request. */
	static bool restartRequested; /**< Pending restart request. */
	static Thread *handoffTarget; /**< Thread to run next, if any. */
	static uintptr_t syscallFrame; /**< Saved context of the current call. */
	/** @brief Moves every ready thread to the highest priority. */
	static void boostPriorities();
	/**
	 * @brief Puts a thread on the ready queue of its priority.
	 * @param thread Thread to enqueue, ignored if already queued.
	 */
	static void enqueue(Thread *thread);
	/** @return The highest priority ready thread, removed from its queue, or
	 * nullptr. */
	static Thread *dequeueNext();
	/** @return True when any ready queue is not empty. */
	static bool hasReady();
	/**
	 * @brief Saves the current thread, picks the next one and installs it.
	 * @param current_stack_pointer Saved context of the current thread.
	 * @param quantum_expired True when called because the quantum ran out,
	 * which lowers the priority of the preempted thread.
	 * @return Saved context of the thread to resume.
	 */
	static uintptr_t switchFrom(uintptr_t current_stack_pointer,
								bool quantum_expired);
};

} // namespace cock::core::task

#endif // !CORE_SCHEDULER_HPP
