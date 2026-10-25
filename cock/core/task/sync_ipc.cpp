#include <cock/core/hal/interrupt_guard.hpp>
#include <cock/core/hal/tasking.hpp>
#include <cock/core/task/endpoint_table.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/sync_ipc.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/timeouts.hpp>

namespace cock::core::task {

using hal::InterruptGuard;
using hal::SyscallFrame;

namespace {

constexpr size_t PEER_VALUE = 0;
constexpr size_t FIRST_WORD_VALUE = 1;

SyscallFrame frameWith(uintptr_t result, uint32_t peer,
					   const uint32_t (&words)[abi::call::WORD_COUNT]) {
	SyscallFrame frame = {};
	frame.result = result;
	frame.values[PEER_VALUE] = peer;
	for (size_t index = 0; index < abi::call::WORD_COUNT; index++)
		frame.values[FIRST_WORD_VALUE + index] = words[index];
	return frame;
}

bool isForever(uint32_t milliseconds) {
	return milliseconds == abi::call::NO_TIMEOUT;
}

Thread *endpointCallers[abi::endpoint::MAX_ENDPOINTS] = {};
size_t queuedCallers = 0;

Thread *&queueOf(uint32_t endpoint) {
	return endpointCallers[endpoint - abi::endpoint::FIRST_ENDPOINT];
}

} // namespace

bool SyncIpc::hasCallers(const Thread &server) {
	if (server.callers != nullptr) return true;
	if (queuedCallers == 0 || !server.isMainThread() || server.getProcess()->isKernel())
		return false;

	uint32_t owner = server.getProcess()->getId();
	for (size_t index = 0; index < abi::endpoint::MAX_ENDPOINTS; index++) {
		uint32_t endpoint = abi::endpoint::FIRST_ENDPOINT + static_cast<uint32_t>(index);
		if (endpointCallers[index] != nullptr && endpoint_table.ownerOf(endpoint) == owner)
			return true;
	}
	return false;
}

Thread *SyncIpc::takeCaller(Thread &server) {
	Thread *caller = server.callers;
	if (caller != nullptr) {
		server.callers = caller->callLink;
		caller->callLink = nullptr;
		return caller;
	}
	if (queuedCallers == 0 || !server.isMainThread() || server.getProcess()->isKernel())
		return nullptr;

	uint32_t owner = server.getProcess()->getId();
	for (size_t index = 0; index < abi::endpoint::MAX_ENDPOINTS; index++) {
		uint32_t endpoint = abi::endpoint::FIRST_ENDPOINT + static_cast<uint32_t>(index);
		Thread *&head = endpointCallers[index];
		if (head == nullptr || endpoint_table.ownerOf(endpoint) != owner) continue;

		caller = head;
		head = caller->callLink;
		caller->callLink = nullptr;
		caller->callEndpoint = abi::endpoint::NO_ENDPOINT;
		queuedCallers--;
		return caller;
	}
	return nullptr;
}

bool SyncIpc::leaveEndpoint(Thread &caller) {
	if (caller.callEndpoint == abi::endpoint::NO_ENDPOINT) return false;

	bool removed = unlink(queueOf(caller.callEndpoint), caller);
	caller.callEndpoint = abi::endpoint::NO_ENDPOINT;
	if (removed) queuedCallers--;
	return removed;
}

uintptr_t SyncIpc::frameOf(Thread &thread) {
	if (&thread == Scheduler::getCurrentThread()) return Scheduler::getSyscallFrame();
	return thread.getStackPointer();
}

void SyncIpc::writeResult(Thread &thread, uintptr_t result) {
	SyscallFrame frame = {};
	hal::read_syscall_frame(frameOf(thread), frame);
	frame.result = result;
	hal::write_syscall_frame(frameOf(thread), frame);
}

void SyncIpc::dispatch(Thread &server) {
	Thread *caller = takeCaller(server);
	caller->callTarget = &server;
	server.serving = caller;
	Timeouts::cancel(&server);

	SyscallFrame frame = frameWith(abi::call::SUCCESS, caller->callPeer, caller->callWords);
	hal::write_syscall_frame(frameOf(server), frame);
}

void SyncIpc::finish(Thread *caller, uintptr_t result) {
	const uint32_t none[abi::call::WORD_COUNT] = {};
	SyscallFrame frame = frameWith(result, 0, none);
	hal::write_syscall_frame(caller->getStackPointer(), frame);
	caller->callTarget = nullptr;
	caller->callEndpoint = abi::endpoint::NO_ENDPOINT;
	Timeouts::cancel(caller);
	caller->setState(ThreadState::READY);
	Scheduler::addThread(caller);
}

CallStart SyncIpc::call(Thread &caller, uint32_t target_id,
						const uint32_t (&words)[abi::call::WORD_COUNT],
						uint32_t milliseconds, uint32_t peer) {
	InterruptGuard guard;
	Thread *target = TaskManager::getThreadByPID(target_id);
	if (target == nullptr || target == &caller ||
		target->getState() == ThreadState::DEAD)
		return CallStart::REFUSED;
	if (milliseconds == 0) return CallStart::TIMED_OUT;

	for (size_t index = 0; index < abi::call::WORD_COUNT; index++)
		caller.callWords[index] = words[index];
	caller.callLink = nullptr;
	caller.callTarget = target;
	caller.callEndpoint = abi::endpoint::NO_ENDPOINT;
	caller.callPeer = peer;
	caller.setState(ThreadState::WAITING_REPLY);
	if (!isForever(milliseconds))
		Timeouts::arm(&caller, Timeouts::ticksFor(milliseconds));

	Thread **link = &target->callers;
	while (*link != nullptr)
		link = &(*link)->callLink;
	*link = &caller;

	if (target->getState() == ThreadState::WAITING_CALL) {
		dispatch(*target);
		target->setState(ThreadState::READY);
		Scheduler::requestHandoff(target);
	}
	Scheduler::requestReschedule();
	return CallStart::BLOCKED;
}

CallStart SyncIpc::callThroughEndpoint(Thread &caller, uint32_t endpoint,
									   const uint32_t (&words)[abi::call::WORD_COUNT],
									   uint32_t milliseconds, uint32_t peer) {
	InterruptGuard guard;
	if (!endpoint_table.exists(endpoint)) return CallStart::REFUSED;
	Thread *owner = TaskManager::getThreadByPID(endpoint_table.ownerOf(endpoint));
	if (owner == &caller) return CallStart::REFUSED;
	if (milliseconds == 0) return CallStart::TIMED_OUT;

	for (size_t index = 0; index < abi::call::WORD_COUNT; index++)
		caller.callWords[index] = words[index];
	caller.callLink = nullptr;
	caller.callTarget = nullptr;
	caller.callEndpoint = endpoint;
	caller.callPeer = peer;
	caller.setState(ThreadState::WAITING_REPLY);
	if (!isForever(milliseconds))
		Timeouts::arm(&caller, Timeouts::ticksFor(milliseconds));

	Thread **link = &queueOf(endpoint);
	while (*link != nullptr)
		link = &(*link)->callLink;
	*link = &caller;
	queuedCallers++;

	if (owner != nullptr && owner->getState() == ThreadState::WAITING_CALL) {
		dispatch(*owner);
		owner->setState(ThreadState::READY);
		Scheduler::requestHandoff(owner);
	}
	Scheduler::requestReschedule();
	return CallStart::BLOCKED;
}

void SyncIpc::resume(Thread &server) {
	InterruptGuard guard;
	dispatch(server);
	server.setState(ThreadState::READY);
	Scheduler::addThread(&server);
}

AcceptResult SyncIpc::accept(Thread &server, uint32_t milliseconds) {
	InterruptGuard guard;
	if (server.serving != nullptr) return AcceptResult::REFUSED;

	if (hasCallers(server)) {
		dispatch(server);
		return AcceptResult::SERVED;
	}
	if (server.hasMessages() || server.hasNotifications() || server.hasExitNotices())
		return AcceptResult::PENDING;
	if (milliseconds == 0) return AcceptResult::TIMED_OUT;

	server.setState(ThreadState::WAITING_CALL);
	if (!isForever(milliseconds))
		Timeouts::arm(&server, Timeouts::ticksFor(milliseconds));
	Scheduler::requestReschedule();
	return AcceptResult::BLOCKED;
}

bool SyncIpc::unlink(Thread *&head, Thread &thread) {
	Thread **link = &head;
	while (*link != nullptr && *link != &thread)
		link = &(*link)->callLink;
	if (*link != &thread) return false;
	*link = thread.callLink;
	thread.callLink = nullptr;
	return true;
}

void SyncIpc::deliver(Thread &server, Thread &caller,
					  const uint32_t (&words)[abi::call::WORD_COUNT], bool handoff) {
	SyscallFrame frame = frameWith(abi::call::SUCCESS, server.getId(), words);
	hal::write_syscall_frame(caller.getStackPointer(), frame);
	caller.callTarget = nullptr;
	Timeouts::cancel(&caller);
	caller.setState(ThreadState::READY);
	if (handoff) {
		Scheduler::requestHandoff(&caller);
		Scheduler::requestReschedule();
	} else {
		Scheduler::addThread(&caller);
	}
}

bool SyncIpc::reply(Thread &server, const uint32_t (&words)[abi::call::WORD_COUNT],
					bool handoff) {
	InterruptGuard guard;
	Thread *caller = server.serving;
	if (caller == nullptr) return false;
	server.serving = nullptr;

	deliver(server, *caller, words, handoff);
	return true;
}

Thread *SyncIpc::defer(Thread &server) {
	InterruptGuard guard;
	Thread *caller = server.serving;
	if (caller == nullptr) return nullptr;
	server.serving = nullptr;

	caller->callLink = server.deferred;
	server.deferred = caller;
	return caller;
}

bool SyncIpc::replyTo(Thread &server, uint32_t ticket,
					  const uint32_t (&words)[abi::call::WORD_COUNT]) {
	InterruptGuard guard;
	Thread *caller = server.deferred;
	while (caller != nullptr && caller->getId() != ticket)
		caller = caller->callLink;
	if (caller == nullptr) return false;

	unlink(server.deferred, *caller);
	deliver(server, *caller, words, false);
	return true;
}

void SyncIpc::wakeForMessage(Thread &server) {
	InterruptGuard guard;
	writeResult(server, abi::call::MESSAGE_PENDING);
	Timeouts::cancel(&server);
	server.setState(ThreadState::READY);
	Scheduler::addThread(&server);
}

void SyncIpc::expire(Thread &thread) {
	InterruptGuard guard;
	thread.setTimeoutState(TimeoutState::NONE);

	if (thread.getState() == ThreadState::WAITING_CALL) {
		writeResult(thread, abi::call::TIMED_OUT);
		thread.setState(ThreadState::READY);
		Scheduler::addThread(&thread);
		return;
	}
	if (thread.getState() != ThreadState::WAITING_REPLY) return;

	if (thread.callEndpoint != abi::endpoint::NO_ENDPOINT) {
		leaveEndpoint(thread);
		finish(&thread, abi::call::TIMED_OUT);
		return;
	}

	Thread *target = thread.callTarget;
	if (target != nullptr) {
		if (target->serving == &thread) {
			target->serving = nullptr;
		} else if (!unlink(target->callers, thread)) {
			unlink(target->deferred, thread);
		}
	}
	thread.callLink = nullptr;
	finish(&thread, abi::call::TIMED_OUT);
}

void SyncIpc::withdraw(Thread &caller) {
	InterruptGuard guard;
	if (caller.getState() != ThreadState::WAITING_REPLY) return;

	leaveEndpoint(caller);
	Thread *target = caller.callTarget;
	if (target != nullptr) {
		if (target->serving == &caller) {
			target->serving = nullptr;
		} else if (!unlink(target->callers, caller)) {
			unlink(target->deferred, caller);
		}
	}
	caller.callLink = nullptr;
	caller.callTarget = nullptr;
}

void SyncIpc::abandon(Thread &dying) {
	InterruptGuard guard;
	leaveEndpoint(dying);
	while (dying.callers != nullptr) {
		Thread *caller = dying.callers;
		dying.callers = caller->callLink;
		caller->callLink = nullptr;
		finish(caller, abi::call::FAILURE);
	}
	while (dying.deferred != nullptr) {
		Thread *caller = dying.deferred;
		dying.deferred = caller->callLink;
		caller->callLink = nullptr;
		finish(caller, abi::call::FAILURE);
	}
	if (dying.serving != nullptr) {
		Thread *caller = dying.serving;
		dying.serving = nullptr;
		finish(caller, abi::call::FAILURE);
	}
}

} // namespace cock::core::task
