#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::task {

using cock::utils::Logger;
using hal::InterruptGuard;

Thread *TaskManager::registry[MAX_PROCESSES];
uint32_t TaskManager::freePids[MAX_PROCESSES];
size_t TaskManager::freePidCount = 0;
ThreadQueue TaskManager::graveyard;
Process *TaskManager::kernelProcess = nullptr;

void TaskManager::init() {
	InterruptGuard guard;

	memset(registry, 0, sizeof(registry));
	freePidCount = 0;

	for (uint32_t pid = MAX_PROCESSES; pid > 0; pid--)
		freePids[freePidCount++] = pid - 1;

	if (kernelProcess == nullptr) kernelProcess = Process::createKernel();

	Logger::debug("TaskManager initialized.");
}

Process *TaskManager::getKernelProcess() { return kernelProcess; }

uint32_t TaskManager::allocatePID() {
	InterruptGuard guard;

	if (freePidCount == 0) utils::panic("TaskManager: Out of PIDs");
	return freePids[--freePidCount];
}

bool TaskManager::registerThread(Thread *thread) {
	if (!thread || thread->getId() >= MAX_PROCESSES) return false;

	InterruptGuard guard;
	uint32_t pid = thread->getId();
	if (registry[pid] != nullptr) return false;
	registry[pid] = thread;
	return true;
}

Thread *TaskManager::getThreadByPID(uint32_t pid) {
	return (pid < MAX_PROCESSES) ? registry[pid] : nullptr;
}

void TaskManager::notifyThreadDeath(Thread *thread) {
	if (!thread) return;

	InterruptGuard guard;
	graveyard.push(thread);
}

void TaskManager::buryDeadThreads() {
	for (;;) {
		Thread *thread = nullptr;
		{
			InterruptGuard guard;
			thread = graveyard.pop();
			if (!thread) return;
			registry[thread->getId()] = nullptr;
		}

		uint32_t pid = thread->getId();
		delete thread;

		InterruptGuard guard;
		freePids[freePidCount++] = pid;
	}
}

} // namespace cock::core::task
