/** @file
 * @brief Registry of every thread, process id allocation and thread reaping.
 */
#ifndef CORE_TASK_MANAGER_HPP
#define CORE_TASK_MANAGER_HPP

#include <cock/core/task/process.hpp>
#include <cock/core/task/thread.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

/**
 * @brief Global registry and lifecycle manager for all system threads.
 * * Handles Process ID (PID) allocation, thread registration, and garbage
 * collection.
 *
 * A pid packs a slot in the low bits and a generation in the high bits. The
 * generation advances every time a slot is reaped, so a stale pid never names
 * a newer thread. Mutating operations block interrupts while they run.
 */
class TaskManager {
  private:
	/** @brief Maximum number of simultaneous threads supported by the OS. */
	static constexpr uint32_t SLOT_BITS = 12;
	/** @brief Number of pid slots, which is also the thread limit. */
	static constexpr size_t MAX_PROCESSES = size_t(1) << SLOT_BITS;
	/** @brief Mask that extracts the slot from a pid. */
	static constexpr uint32_t SLOT_MASK = MAX_PROCESSES - 1;
	/** @brief Bits of a pid used by the generation counter. */
	static constexpr uint32_t GENERATION_BITS = 19;
	/** @brief Mask of a generation value, after which it wraps. */
	static constexpr uint32_t GENERATION_MASK = (uint32_t(1) << GENERATION_BITS) - 1;

	/** @brief Array mapping PID slots to their respective Thread object pointers. */
	static Thread *registry[MAX_PROCESSES];
	static uint32_t freePids[MAX_PROCESSES]; /**< Stack of free slots. */
	static uint32_t generations[MAX_PROCESSES]; /**< Current generation per slot. */
	static size_t freePidCount; /**< Number of entries in freePids. */
	static ThreadQueue graveyard; /**< Dead threads waiting to be reaped. */
	static Process *kernelProcess; /**< The process of all kernel threads. */
	static Thread *reaper; /**< Thread that calls buryDeadThreads, woken on death. */
	/**
	 * @brief Sends the parent the exit notice of a finished process, if it
	 * asked for one.
	 * @param process Process whose last thread died.
	 */
	static void announceExit(Process &process);

  public:
	/**
	 * @brief Initializes the Task Manager, populating the free PID pool.
	 * @note Creates the kernel process the first time. Call once, early.
	 */
	static void init();

	/** @return The kernel process shared by all kernel threads. */
	static Process *getKernelProcess();
	/**
	 * @brief Allocates the next available unique Process ID: the generation of
	 * the slot in the high bits and the slot in the low bits, so a recycled slot
	 * never yields an id that was handed out before.
	 * @return A uint32_t PID below 2^31. Panics if no PIDs are available.
	 */
	static uint32_t allocatePID();
	/**
	 * @brief Tells whether allocatePID would succeed.
	 * @return True when at least one slot is free. Callers that serve user
	 * requests check this first instead of triggering the panic.
	 */
	static bool hasFreePid();
	/**
	 * @brief Extracts the registry slot of a pid.
	 * @param pid Process or thread id.
	 * @return The slot, the pid without its generation.
	 */
	static constexpr uint32_t slotOf(uint32_t pid) { return pid & SLOT_MASK; }

	/**
	 * @brief Registers a newly created thread into the global registry.
	 * @param thread Pointer to the thread to register.
	 * @return true if successfully registered, false if the PID is invalid or
	 * taken.
	 */
	static bool registerThread(Thread *thread);

	/**
	 * @brief Looks up a thread by its PID.
	 * @param pid The Process ID to search for. Any value is safe, since the
	 * generation is compared.
	 * @return Pointer to the Thread, or nullptr if it does not exist.
	 */
	static Thread *getThreadByPID(uint32_t pid);

	/**
	 * @brief Finds a live process that the kernel started from a boot module, by name.
	 * @param name Null terminated module name.
	 * @return The process id of the first match, or all ones when none exists.
	 * The kernel process, dead threads and every process that was not started
	 * from a boot module are skipped, only the main thread of a process
	 * identifies it.
	 */
	static uint32_t findBootModuleId(const char *name);
	/**
	 * @brief Sets the thread that reaps the dead.
	 * @param thread Reaper thread, woken when a thread dies while it sleeps.
	 */
	static void setReaper(Thread *thread);
	/** @return True when no dead thread waits to be reaped. */
	static bool graveyardEmpty();
	/**
	 * @brief Records the death of a thread.
	 *
	 * Fails the synchronous calls tied to it, queues it for reaping, announces
	 * the exit of its process when it was the last thread and wakes the reaper.
	 * @param thread Thread that was marked dead, null is ignored.
	 */
	static void notifyThreadDeath(Thread *thread);
	/**
	 * @brief Destroys every thread in the graveyard and frees its slot.
	 * @note Called by the reaper in process context. Bumps the generation of
	 * each slot before it is reused.
	 */
	static void buryDeadThreads();
};

} // namespace cock::core::task

#endif // !CORE_TASK_MANAGER_HPP
