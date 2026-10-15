#ifndef CORE_TASK_MANAGER_HPP
#define CORE_TASK_MANAGER_HPP

#include <cock/core/task/thread.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

class TaskManager {
  private:
	static constexpr size_t MAX_PROCESSES = 4096;
	static Thread *registry[MAX_PROCESSES];
	static uint32_t freePids[MAX_PROCESSES];
	static size_t freePidCount;
	static ThreadQueue graveyard;

  public:
	static void init();
	static uint32_t allocatePID();
	static bool registerThread(Thread *thread);
	static Thread *getThreadByPID(uint32_t pid);
	static void notifyThreadDeath(Thread *thread);
	static void buryDeadThreads();
};

} // namespace cock::core::task

#endif // !CORE_TASK_MANAGER_HPP
