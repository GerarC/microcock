#include <cock/abi/process.hpp>
#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/sched_metrics.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/sync_ipc.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::task {

using cock::utils::Logger;
using hal::InterruptGuard;

constexpr uint32_t NO_PROCESS = 0xFFFFFFFF;

Thread *TaskManager::registry[MAX_PROCESSES];
uint32_t TaskManager::freePids[MAX_PROCESSES];
uint32_t TaskManager::generations[MAX_PROCESSES];
size_t TaskManager::freePidCount = 0;
ThreadQueue TaskManager::graveyard;
Process *TaskManager::kernelProcess = nullptr;
Thread *TaskManager::reaper = nullptr;

void TaskManager::init() {
	InterruptGuard guard;

	memset(registry, 0, sizeof(registry));
	memset(generations, 0, sizeof(generations));
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
	uint32_t slot = freePids[--freePidCount];
	return (generations[slot] << SLOT_BITS) | slot;
}

bool TaskManager::hasFreePid() {
	InterruptGuard guard;
	return freePidCount > 0;
}

bool TaskManager::registerThread(Thread *thread) {
	if (!thread || (thread->getId() >> SLOT_BITS) > GENERATION_MASK) return false;

	InterruptGuard guard;
	uint32_t pid = thread->getId();
	uint32_t slot = pid & SLOT_MASK;
	if ((pid >> SLOT_BITS) != generations[slot]) return false;
	if (registry[slot] != nullptr) return false;
	registry[slot] = thread;
	return true;
}

Thread *TaskManager::getThreadByPID(uint32_t pid) {
	Thread *thread = registry[pid & SLOT_MASK];
	return (thread && thread->getId() == pid) ? thread : nullptr;
}

namespace {

constexpr uint32_t NO_BITS = 0;

uint32_t stateBit(ThreadState state) {
	switch (state) {
	case ThreadState::READY:
		return abi::process::THREAD_READY;
	case ThreadState::RUNNING:
		return abi::process::THREAD_RUNNING;
	case ThreadState::BLOCKED:
		return abi::process::THREAD_BLOCKED;
	case ThreadState::WAITING_MSG:
		return abi::process::THREAD_WAITING_MSG;
	case ThreadState::WAITING_CALL:
		return abi::process::THREAD_WAITING_CALL;
	case ThreadState::WAITING_REPLY:
		return abi::process::THREAD_WAITING_REPLY;
	case ThreadState::DEAD:
		return NO_BITS;
	}
	return NO_BITS;
}

} // namespace

uint32_t TaskManager::threadStateMask(const Process &process) {
	InterruptGuard guard;

	uint32_t mask = NO_BITS;
	size_t remaining = process.getThreadCount();
	for (size_t slot = 0; slot < MAX_PROCESSES && remaining != 0; slot++) {
		Thread *thread = registry[slot];
		if (!thread || thread->getProcess() != &process) continue;
		remaining--;
		mask |= stateBit(thread->getState());
	}
	return mask;
}

uint32_t TaskManager::findBootModuleId(const char *name) {
	InterruptGuard guard;

	for (size_t slot = 0; slot < MAX_PROCESSES; slot++) {
		Thread *thread = registry[slot];
		if (!thread || thread->getState() == ThreadState::DEAD) continue;

		Process *process = thread->getProcess();
		if (process->isKernel() || !process->isBootModule() ||
			process->getId() != thread->getId())
			continue;
		if (strcmp(process->getName(), name) == 0) return process->getId();
	}
	return NO_PROCESS;
}

void TaskManager::terminateSiblings(Thread &keep) {
	InterruptGuard guard;

	Process *process = keep.getProcess();
	for (size_t slot = 0; slot < MAX_PROCESSES; slot++) {
		Thread *thread = registry[slot];
		if (!thread || thread == &keep || thread->getProcess() != process) continue;
		Scheduler::terminateThread(thread);
	}
}

void TaskManager::terminateProcess(Process &process) {
	InterruptGuard guard;

	for (size_t slot = 0; slot < MAX_PROCESSES; slot++) {
		Thread *thread = registry[slot];
		if (!thread || thread->getProcess() != &process) continue;
		Scheduler::terminateThread(thread);
	}
}

void TaskManager::setReaper(Thread *thread) { reaper = thread; }

bool TaskManager::graveyardEmpty() { return graveyard.isEmpty(); }

void TaskManager::announceExit(Process &process) {
	if (!process.notifiesExit() || process.getParentId() == abi::process::NO_PARENT)
		return;

	if (!IPC::notifyExit(process.getParentId(), process.getId(), process.getExitStatus()))
		Logger::trace("Exit notice of %u was not delivered", process.getId());
}

void TaskManager::notifyThreadDeath(Thread *thread) {
	if (!thread) return;

	InterruptGuard guard;
	SchedMetrics::onExit(*thread);
	SyncIpc::abandon(*thread);
	graveyard.push(thread);

	Process *process = thread->getProcess();
	if (process != nullptr && !process->isKernel() && process->threadExited())
		announceExit(*process);

	if (reaper != nullptr && reaper->getState() == ThreadState::BLOCKED) {
		reaper->setState(ThreadState::READY);
		Scheduler::addThread(reaper);
	}
}

void TaskManager::buryDeadThreads() {
	for (;;) {
		Thread *thread = nullptr;
		{
			InterruptGuard guard;
			thread = graveyard.pop();
			if (!thread) return;
			registry[thread->getId() & SLOT_MASK] = nullptr;
		}

		uint32_t pid = thread->getId();
		uint32_t slot = pid & SLOT_MASK;
		delete thread;
		Logger::debug("Thread %u reaped", pid);

		InterruptGuard guard;
		generations[slot] = (generations[slot] + 1) & GENERATION_MASK;
		freePids[freePidCount++] = slot;
	}
}

} // namespace cock::core::task
