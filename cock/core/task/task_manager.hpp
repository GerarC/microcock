#ifndef CORE_TASK_MANAGER_HPP
#define CORE_TASK_MANAGER_HPP

#include <cock/core/task/thread.hpp>
#include <cock/data_structure/ll.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

using data_structure::LinkedList;

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

	/** @brief Pool of available PIDs ready to be assigned. */
	static LinkedList<uint32_t> freePIDs;

	/** @brief Queue of PIDs belonging to dead threads awaiting memory cleanup.
	 */
	static LinkedList<uint32_t> graveyard;

  public:
	/**
	 * @brief Initializes the Task Manager, populating the free PID pool.
	 */
	static void init();

	/**
	 * @brief Allocates the next available unique Process ID.
	 * @return A unique uint32_t PID. Panics if no PIDs are available.
	 */
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

	/**
	 * @brief Marks a thread as dead and adds it to the graveyard for cleanup.
	 * @param pid The Process ID of the dead thread.
	 */
	static void notifyThreadDeath(uint32_t pid);

	/**
	 * @brief Garbage collector routine.
	 * * Deletes Thread objects in the graveyard and recycles their PIDs.
	 */
	static void buryDeadThreads();
};

} // namespace cock::core::task

#endif // !CORE_TASK_MANAGER_HPP
