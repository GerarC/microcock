#include <cock/core/hal/utils.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/sync_ipc.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>

namespace cock::core::task {

bool IPC::deliver(uint32_t to_pid, Message message) {
	Thread *sender = Scheduler::getCurrentThread();
	return deliverAs(to_pid, message, sender ? sender->getId() : 0);
}

bool IPC::deliverAs(uint32_t to_pid, Message message, uint32_t sender_pid) {
	uintptr_t flags = hal::save_and_block_interruptions();
	Thread *target = TaskManager::getThreadByPID(to_pid);

	if (!target || target->getState() == ThreadState::DEAD) {
		hal::restore_interruptions(flags);
		return false;
	}
	message.sender_pid = sender_pid;
	message.target_pid = target->getId();

	if (!target->receiveMessage(message)) {
		hal::restore_interruptions(flags);
		return false;
	}

	if (target->getState() == ThreadState::WAITING_MSG) {
		target->setState(ThreadState::READY);
		Scheduler::addThread(target);
	} else if (target->getState() == ThreadState::WAITING_CALL) {
		SyncIpc::wakeForMessage(*target);
	}
	hal::restore_interruptions(flags);
	return true;
}

bool IPC::notify(uint32_t to_pid, uint32_t line) {
	uintptr_t flags = hal::save_and_block_interruptions();
	Thread *target = TaskManager::getThreadByPID(to_pid);

	if (!target || target->getState() == ThreadState::DEAD) {
		hal::restore_interruptions(flags);
		return false;
	}

	target->raiseNotification(line);
	if (target->getState() == ThreadState::WAITING_MSG) {
		target->setState(ThreadState::READY);
		Scheduler::addThread(target);
	} else if (target->getState() == ThreadState::WAITING_CALL) {
		SyncIpc::wakeForMessage(*target);
	}
	hal::restore_interruptions(flags);
	return true;
}

void IPC::send(uint32_t to_pid, Message message) {
	if (deliver(to_pid, message)) Thread::yield();
}

void IPC::receive(Message *message) {
	Thread *target = Scheduler::getCurrentThread();
	if (!target || !message) return;

	for (;;) {
		uintptr_t flags = hal::save_and_block_interruptions();

		if (target->getNextMessage(*message)) {
			hal::restore_interruptions(flags);
			return;
		}

		target->setState(ThreadState::WAITING_MSG);
		hal::restore_interruptions(flags);
		Thread::yield();
	}
}

} // namespace cock::core::task
