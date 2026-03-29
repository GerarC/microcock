#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <cock/core/task/task_manager.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::task {

using cock::utils::Logger;
Thread *TaskManager::registry[MAX_PROCESSES];
LinkedList<uint32_t> TaskManager::freePIDs;
LinkedList<uint32_t> TaskManager::graveyard;

void TaskManager::init() {
	memset(registry, 0, MAX_PROCESSES);
	freePIDs.clear();

	for (uint32_t pid = 0; pid < MAX_PROCESSES; pid++)
		freePIDs.append(pid);

	Logger::debug("TaskManager initialized.");
}


uint32_t TaskManager::allocatePID() {
	uint32_t pid;
	if (freePIDs.popFront(pid)) return pid;
	utils::panic("TaskManager: Out of PIDs");
	return 0;
}

bool TaskManager::registerThread(Thread *thread) {
	if (!thread || thread->getId() >= MAX_PROCESSES) return false;
	uint32_t pid = thread->getId();
	if (registry[pid] != nullptr) return false;
	registry[pid] = thread;
	return true;
}

Thread *TaskManager::getThreadByPID(uint32_t pid) {
	return (pid < MAX_PROCESSES) ? registry[pid] : nullptr;
}

void TaskManager::notifyThreadDeath(uint32_t pid){
    graveyard.append(pid);
}

void TaskManager::buryDeadThreads() {
	uint32_t dead_pid;
	while (freePIDs.popFront(dead_pid)) {
		Thread *thread = registry[dead_pid];
		delete thread;
		freePIDs.append(dead_pid);
	}
}

} // namespace cock::core::task
