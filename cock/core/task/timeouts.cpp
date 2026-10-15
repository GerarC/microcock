#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/timeouts.hpp>

namespace cock::core::task {

using hal::InterruptGuard;

uint64_t Timeouts::ticks = 0;
Thread *Timeouts::armed = nullptr;

uint64_t Timeouts::ticksFor(uint32_t milliseconds) {
	uint64_t scaled = static_cast<uint64_t>(milliseconds) * hal::TIMER_FREQUENCY_HZ;
	return (scaled + MILLISECONDS_PER_SECOND - 1) / MILLISECONDS_PER_SECOND;
}

size_t Timeouts::armedCount() {
	InterruptGuard guard;
	size_t count = 0;
	for (Thread *thread = armed; thread != nullptr; thread = thread->timerLink)
		count++;
	return count;
}

void Timeouts::arm(Thread *thread, uint64_t delay) {
	if (thread == nullptr) return;

	InterruptGuard guard;
	cancel(thread);
	thread->setWakeTick(ticks + delay + TICK_MARGIN);
	thread->setTimeoutState(TimeoutState::ARMED);
	thread->timerLink = armed;
	armed = thread;
}

void Timeouts::cancel(Thread *thread) {
	if (thread == nullptr) return;

	InterruptGuard guard;
	if (thread->getTimeoutState() == TimeoutState::ARMED) {
		Thread **link = &armed;
		while (*link != nullptr && *link != thread)
			link = &(*link)->timerLink;
		if (*link == thread) *link = thread->timerLink;
	}
	thread->timerLink = nullptr;
	thread->setTimeoutState(TimeoutState::NONE);
}

void Timeouts::onTick() {
	ticks++;

	Thread **link = &armed;
	while (*link != nullptr) {
		Thread *thread = *link;
		if (thread->getWakeTick() > ticks) {
			link = &thread->timerLink;
			continue;
		}

		*link = thread->timerLink;
		thread->timerLink = nullptr;
		thread->setTimeoutState(TimeoutState::EXPIRED);

		ThreadState state = thread->getState();
		if (state == ThreadState::WAITING_MSG || state == ThreadState::BLOCKED) {
			thread->setState(ThreadState::READY);
			Scheduler::addThread(thread);
		}
	}
}

TimeoutPhase Timeouts::wait(Thread &thread, uint32_t milliseconds,
							ThreadState blockedState) {
	InterruptGuard guard;
	TimeoutState state = thread.getTimeoutState();
	if (state == TimeoutState::EXPIRED) {
		cancel(&thread);
		return TimeoutPhase::TIMED_OUT;
	}
	if (state == TimeoutState::NONE) {
		if (milliseconds == 0) return TimeoutPhase::TIMED_OUT;
		arm(&thread, ticksFor(milliseconds));
	}

	thread.setState(blockedState);
	Scheduler::requestReschedule();
	Scheduler::requestRestart();
	return TimeoutPhase::WAITING;
}

} // namespace cock::core::task
