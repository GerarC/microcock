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
	static constexpr size_t MAX_PROCESSES = 4096;

	/** @brief Array mapping PIDs to their respective Thread object pointers. */
	static Thread *registry[MAX_PROCESSES];
	static uint32_t freePids[MAX_PROCESSES];
	static size_t freePidCount;
	static ThreadQueue graveyard;
	static Process *kernelProcess;

  public:
	/**
	 * @brief Initializes the Task Manager, populating the free PID pool.
	 */
	static void init();

	/**
	 * @brief Allocates the next available unique Process ID.
	 * @return A unique uint32_t PID. Panics if no PIDs are available.
	 */
	static Process *getKernelProcess();
	static uint32_t allocatePID();

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
	static void notifyThreadDeath(Thread *thread);
	static void buryDeadThreads();
};

} // namespace cock::core::task

#endif // !CORE_TASK_MANAGER_HPP
