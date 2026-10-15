#include <cock/core/hal/utils.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>

namespace cock::core::task {

void IPC::send(uint32_t to_pid, Message message) {
	uintptr_t flags = hal::save_and_block_interruptions();
	Thread *target = TaskManager::getThreadByPID(to_pid);
	Thread *sender = Scheduler::getCurrentThread();

	if (!sender || !target || target->getState() == ThreadState::DEAD) {
		hal::restore_interruptions(flags);
		return;
	}
	message.sender_pid = sender->getId();
	message.target_pid = target->getId();

	target->receiveMessage(message);

	if (target->getState() == ThreadState::WAITING_MSG) {
		target->setState(ThreadState::READY);
		Scheduler::addThread(target);
		hal::restore_interruptions(flags);
		Thread::yield();
	} else hal::restore_interruptions(flags);
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
