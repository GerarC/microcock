#include <cock/core/task/task_manager.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

using utils::Logger;

LinkedList<Thread *> Scheduler::readyQueues[PRIORITY_NUMBER];
Thread *Scheduler::currentThread = nullptr;

uint32_t Scheduler::currentTicks = 0;
uint32_t Scheduler::ticksSinceLastBoost = 0;
bool Scheduler::initialized = false;

void Scheduler::init() {
	currentThread = nullptr;
	currentTicks = 0;
	ticksSinceLastBoost = 0;
	initialized = true;
	Logger::debug("Scheduler Initialized with MLFQ");
}

void Scheduler::addThread(Thread *thread) {
	if (!thread) return;
	uint8_t priority = static_cast<uint8_t>(thread->getPriority());
	readyQueues[priority].append(thread);
};

Thread *Scheduler::getCurrentThread() { return currentThread; }

void Scheduler::exitCurrentThread(ThreadResult result) {
	if (!currentThread) return;

	if (result.hasError())
		Logger::error("Thread %d exited with error!", currentThread->getId());
	else
		Logger::trace("Thread %d finished successfully.",
					  currentThread->getId());

	currentThread->setState(ThreadState::DEAD);
	TaskManager::notifyThreadDeath(currentThread->getId());

	Scheduler::yield();
}

void Scheduler::yield() {
	if (currentThread && currentThread->getState() == ThreadState::RUNNING)
		currentThread->setState(ThreadState::READY);
	hal::yield();
}

void Scheduler::boostPriorities() {
	ThreadPriority max_priority =
		static_cast<ThreadPriority>(PRIORITY_NUMBER - 1);
	for (size_t p = 0; p < PRIORITY_NUMBER - 1; p++) {
		Thread *thread = nullptr;
		while (readyQueues[p].popFront(thread)) {
			thread->setPriority(max_priority);
			readyQueues[PRIORITY_NUMBER - 1].append(thread);
		}
	}
	Logger::trace("[MLFQ] Priority Boost: All threads moved to CRITICAL");
}

uintptr_t Scheduler::schedule(uintptr_t current_stack_pointer) {
	if (!initialized) return current_stack_pointer;
	ticksSinceLastBoost++;

	if (ticksSinceLastBoost >= BOOST_TICKS) {
		boostPriorities();
		ticksSinceLastBoost = 0;
	}

	if (currentThread) {
		currentThread->setStackPointer(current_stack_pointer);
		if (currentThread->getState() == ThreadState::RUNNING) {
			currentTicks++;
			if (currentTicks < QUANTUM) return current_stack_pointer;
			uint8_t current_priority =
				static_cast<uint8_t>(currentThread->getPriority());
			if (current_priority > 0) {
				currentThread->setPriority(
					static_cast<ThreadPriority>(current_priority - 1));
			}
			currentThread->setState(ThreadState::READY);
			addThread(currentThread);
		} else if (currentThread->getState() == ThreadState::READY) {
			addThread(currentThread);
		}
	}

	currentThread = nullptr;
	for (int priority = PRIORITY_NUMBER - 1; priority >= 0; priority--) {
		if (readyQueues[priority].popFront(currentThread)) break;
	}

	if (!currentThread) utils::panic("No runnable threads! System Halted.");

	currentThread->setState(ThreadState::RUNNING);
	currentTicks = 0;

	hal::set_kernel_stack(currentThread->getKernelStackTop());
	hal::apply_thread_context(currentThread->getArchContext());
	hal::switch_address_space(currentThread->getAddressSpace());

	return currentThread->getStackPointer();
}

} // namespace cock::core::task
