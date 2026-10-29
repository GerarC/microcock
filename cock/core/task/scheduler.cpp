#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/task/futex.hpp>
#include <cock/core/task/sched_metrics.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/sync_ipc.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/task/timeouts.hpp>
#include <hal/cpu.hpp>
#include <cock/utils/logger.hpp>
#include <cock/utils/panic.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::core::task {

using hal::InterruptGuard;
using utils::Logger;

ThreadQueue Scheduler::readyQueues[PRIORITY_NUMBER];
Thread *Scheduler::currentThread = nullptr;
Thread *Scheduler::idleThread = nullptr;

uint32_t Scheduler::currentTicks = 0;
uint32_t Scheduler::ticksSinceLastBoost = 0;
bool Scheduler::initialized = false;
bool Scheduler::rescheduleRequested = false;
bool Scheduler::restartRequested = false;
Thread *Scheduler::handoffTarget = nullptr;
uintptr_t Scheduler::syscallFrame = 0;
uint64_t Scheduler::lastTickCycles = 0;
uint64_t Scheduler::runStartCycles = 0;
uint32_t Scheduler::cyclesPerTick = 0;

void Scheduler::init() {
	currentThread = nullptr;
	idleThread = nullptr;
	lastTickCycles = 0;
	runStartCycles = 0;
	cyclesPerTick = 0;
	currentTicks = 0;
	ticksSinceLastBoost = 0;
	initialized = true;
	handoffTarget = nullptr;
	Logger::debug("Scheduler Initialized with MLFQ");
}

void Scheduler::setIdleThread(Thread *thread) { idleThread = thread; }

void Scheduler::enqueue(Thread *thread) {
	if (thread->queued || thread->getProcess()->isStopped()) return;
	thread->queued = true;
	SchedMetrics::onReady(*thread);
	readyQueues[static_cast<uint8_t>(thread->getPriority())].push(thread);
}

Thread *Scheduler::dequeueNext() {
	for (int priority = PRIORITY_NUMBER - 1; priority >= 0; priority--) {
		Thread *thread = readyQueues[priority].pop();
		if (!thread) continue;
		thread->queued = false;
		return thread;
	}
	return nullptr;
}

bool Scheduler::hasReady() {
	for (size_t priority = 0; priority < PRIORITY_NUMBER; priority++) {
		if (!readyQueues[priority].isEmpty()) return true;
	}
	return false;
}

void Scheduler::addThread(Thread *thread) {
	if (!thread || thread == idleThread) return;

	InterruptGuard guard;
	enqueue(thread);
}

Thread *Scheduler::getCurrentThread() { return currentThread; }

void Scheduler::terminateCurrentThread() {
	InterruptGuard guard;

	if (!currentThread || currentThread->getState() == ThreadState::DEAD)
		return;
	if (currentThread == idleThread) utils::panic("Idle thread terminated");

	currentThread->setState(ThreadState::DEAD);
	TaskManager::notifyThreadDeath(currentThread);
	requestReschedule();
}

void Scheduler::terminateThread(Thread *thread) {
	InterruptGuard guard;

	if (!thread || thread == currentThread || thread == idleThread ||
		thread->getState() == ThreadState::DEAD)
		return;

	SyncIpc::withdraw(*thread);
	Futex::remove(thread);
	Timeouts::cancel(thread);
	if (thread->queued) {
		readyQueues[static_cast<uint8_t>(thread->getPriority())].remove(thread);
		thread->queued = false;
	}
	thread->setState(ThreadState::DEAD);
	TaskManager::notifyThreadDeath(thread);
}

void Scheduler::hold(Thread *thread) {
	InterruptGuard guard;

	if (!thread || thread == currentThread || thread == idleThread || !thread->queued) return;
	readyQueues[static_cast<uint8_t>(thread->getPriority())].remove(thread);
	thread->queued = false;
}

void Scheduler::release(Thread *thread) {
	InterruptGuard guard;

	if (!thread || thread == idleThread || thread->getState() != ThreadState::READY) return;
	enqueue(thread);
}

void Scheduler::requestReschedule() { rescheduleRequested = true; }

bool Scheduler::takeRescheduleRequest() {
	bool requested = rescheduleRequested;
	rescheduleRequested = false;
	return requested;
}

void Scheduler::requestHandoff(Thread *thread) { handoffTarget = thread; }

void Scheduler::requestRestart() { restartRequested = true; }

bool Scheduler::takeRestartRequest() {
	bool requested = restartRequested;
	restartRequested = false;
	return requested;
}

void Scheduler::exitCurrentThread(ThreadResult result) {
	if (!currentThread) return;

	if (result.hasError())
		Logger::error("Thread %d exited with error!", currentThread->getId());
	else
		Logger::trace("Thread %d finished successfully.",
					  currentThread->getId());

	terminateCurrentThread();
	Scheduler::yield();
}

void Scheduler::yield() {
	if (currentThread && currentThread->getState() == ThreadState::RUNNING)
		currentThread->setState(ThreadState::READY);
	hal::yield();
}

void Scheduler::boostPriorities() {
	size_t max_index = PRIORITY_NUMBER - 1;
	ThreadPriority max_priority = static_cast<ThreadPriority>(max_index);
	for (size_t p = 0; p < max_index; p++) {
		Thread *thread = nullptr;
		while ((thread = readyQueues[p].pop()) != nullptr) {
			thread->setPriority(max_priority);
			readyQueues[max_index].push(thread);
		}
	}
	Logger::trace("[MLFQ] Priority Boost: All threads moved to CRITICAL");
}

uintptr_t Scheduler::onTick(uintptr_t current_stack_pointer) {
	if (!initialized) return current_stack_pointer;

	Timeouts::onTick();
	SchedMetrics::onTick(currentThread, idleThread);

	uint64_t now = hal::read_cycle_counter();
	measureTick(now);
	if (currentThread && currentThread != idleThread &&
		currentThread->getState() == ThreadState::RUNNING) {
		currentThread->chargeLevelTick();
		runStartCycles = now;
	}

	if (++ticksSinceLastBoost >= BOOST_TICKS) {
		boostPriorities();
		ticksSinceLastBoost = 0;
	}

	if (currentThread && currentThread->getState() == ThreadState::RUNNING) {
		if (currentThread == idleThread) {
			if (!hasReady()) return current_stack_pointer;
		} else if (++currentTicks < QUANTUM) {
			return current_stack_pointer;
		}
	}

	return switchFrom(current_stack_pointer);
}

uintptr_t Scheduler::onYield(uintptr_t current_stack_pointer) {
	if (!initialized) return current_stack_pointer;
	return switchFrom(current_stack_pointer);
}

void Scheduler::measureTick(uint64_t now) {
	uint64_t gap = now - lastTickCycles;
	if (lastTickCycles != 0 && gap >= MIN_CYCLES_PER_TICK &&
		gap <= MAX_CYCLES_PER_TICK)
		cyclesPerTick = static_cast<uint32_t>(gap);
	lastTickCycles = now;
}

void Scheduler::chargeRun(Thread &thread, uint64_t now) {
	if (cyclesPerTick == 0) return;
	uint64_t ran = now - runStartCycles;
	if (ran > cyclesPerTick) ran = cyclesPerTick;
	thread.chargeLevelCycles(static_cast<uint32_t>(ran), cyclesPerTick);
}

uintptr_t Scheduler::switchFrom(uintptr_t current_stack_pointer) {
	rescheduleRequested = false;
	Thread *previous = currentThread;
	uint64_t now = hal::read_cycle_counter();

	if (previous) {
		previous->setStackPointer(current_stack_pointer);
		if (previous != idleThread) chargeRun(*previous, now);

		if (previous->getState() == ThreadState::RUNNING)
			previous->setState(ThreadState::READY);

		uint8_t priority = static_cast<uint8_t>(previous->getPriority());
		if (previous != idleThread && priority > 0 &&
			previous->getLevelTicks() >= LEVEL_ALLOWANCE_TICKS)
			previous->setPriority(static_cast<ThreadPriority>(priority - 1));

		if (previous->getState() == ThreadState::READY &&
			previous != idleThread)
			enqueue(previous);
	}

	Thread *next = nullptr;
	if (handoffTarget != nullptr) {
		if (handoffTarget->getState() == ThreadState::READY &&
			!handoffTarget->queued && !handoffTarget->getProcess()->isStopped())
			next = handoffTarget;
		handoffTarget = nullptr;
	}
	if (!next) next = dequeueNext();
	if (!next) next = idleThread;
	if (!next) utils::panic("No runnable threads! System Halted.");

	currentThread = next;
	runStartCycles = now;
	SchedMetrics::onDispatch(*currentThread);
	currentThread->setState(ThreadState::RUNNING);
	currentTicks = 0;

	hal::set_kernel_stack(currentThread->getKernelStackTop());
	Process *process = currentThread->getProcess();
	if (!process->isKernel()) hal::apply_process_context(process->getContext());
	if (hal::get_current_address_space() != process->getAddressSpace())
		hal::switch_address_space(process->getAddressSpace());

	return currentThread->getStackPointer();
}

} // namespace cock::core::task
