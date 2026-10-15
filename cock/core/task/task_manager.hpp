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
 */
class TaskManager {
  private:
	/** @brief Maximum number of simultaneous threads supported by the OS. */
	static constexpr uint32_t SLOT_BITS = 12;
	static constexpr size_t MAX_PROCESSES = size_t(1) << SLOT_BITS;
	static constexpr uint32_t SLOT_MASK = MAX_PROCESSES - 1;
	static constexpr uint32_t GENERATION_BITS = 19;
	static constexpr uint32_t GENERATION_MASK = (uint32_t(1) << GENERATION_BITS) - 1;

	/** @brief Array mapping PID slots to their respective Thread object pointers. */
	static Thread *registry[MAX_PROCESSES];
	static uint32_t freePids[MAX_PROCESSES];
	static uint32_t generations[MAX_PROCESSES];
	static size_t freePidCount;
	static ThreadQueue graveyard;
	static Process *kernelProcess;
	static Thread *reaper;
	static void announceExit(Process &process);

  public:
	/**
	 * @brief Initializes the Task Manager, populating the free PID pool.
	 */
	static void init();

	/**
	 * @brief Allocates the next available unique Process ID: the generation of
	 * the slot in the high bits and the slot in the low bits, so a recycled slot
	 * never yields an id that was handed out before.
	 * @return A uint32_t PID below 2^31. Panics if no PIDs are available.
	 */
	static Process *getKernelProcess();
	static uint32_t allocatePID();
	static bool hasFreePid();
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
	 * @param pid The Process ID to search for.
	 * @return Pointer to the Thread, or nullptr if it does not exist.
	 */
	static Thread *getThreadByPID(uint32_t pid);

	static uint32_t findProcessId(const char *name);
	static void setReaper(Thread *thread);
	static bool graveyardEmpty();
	static void notifyThreadDeath(Thread *thread);
	static void buryDeadThreads();
};

} // namespace cock::core::task

#endif // !CORE_TASK_MANAGER_HPP
