#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/task/futex.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/timeouts.hpp>

namespace cock::core::task {

using hal::InterruptGuard;

Thread *Futex::waiters = nullptr;

void Futex::enqueue(Thread *thread, uintptr_t address) {
	if (thread == nullptr) return;

	InterruptGuard guard;
	remove(thread);
	thread->setFutexAddress(address);
	thread->setFutexWoken(false);
	thread->futexLink = nullptr;

	Thread **link = &waiters;
	while (*link != nullptr)
		link = &(*link)->futexLink;
	*link = thread;
}

void Futex::remove(Thread *thread) {
	if (thread == nullptr) return;

	InterruptGuard guard;
	if (thread->isFutexWaiting()) {
		Thread **link = &waiters;
		while (*link != nullptr && *link != thread)
			link = &(*link)->futexLink;
		if (*link == thread) *link = thread->futexLink;
	}
	thread->futexLink = nullptr;
	thread->setFutexAddress(NO_FUTEX_ADDRESS);
}

size_t Futex::wake(const Process *process, uintptr_t address, size_t count) {
	InterruptGuard guard;
	size_t woken = 0;

	Thread **link = &waiters;
	while (*link != nullptr && woken < count) {
		Thread *thread = *link;
		if (thread->getProcess() != process || thread->getFutexAddress() != address) {
			link = &thread->futexLink;
			continue;
		}

		*link = thread->futexLink;
		thread->futexLink = nullptr;
		thread->setFutexAddress(NO_FUTEX_ADDRESS);
		thread->setFutexWoken(true);
		Timeouts::cancel(thread);
		if (thread->getState() == ThreadState::BLOCKED) {
			thread->setState(ThreadState::READY);
			Scheduler::addThread(thread);
		}
		woken++;
	}
	return woken;
}

size_t Futex::waiterCount() {
	InterruptGuard guard;
	size_t count = 0;
	for (Thread *thread = waiters; thread != nullptr; thread = thread->futexLink)
		count++;
	return count;
}

} // namespace cock::core::task
