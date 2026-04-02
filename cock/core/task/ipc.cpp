#include <cock/core/hal/utils.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>

namespace cock::core::task {

void IPC::send(uint32_t to_pid, Message message) {
	hal::block_interruptions();
	Thread *target = TaskManager::getThreadByPID(to_pid);
	Thread *sender = Scheduler::getCurrentThread();

	if (!target || target->getState() == ThreadState::DEAD) {
		hal::unblock_interruptions();
		return;
	}
	message.sender_pid = sender->getId();
	message.target_pid = target->getId();

	target->receiveMessage(message);

	if (target->getState() == ThreadState::WAITING_MSG) {
		target->setState(ThreadState::READY);
		Scheduler::addThread(target);
		hal::unblock_interruptions();
		Thread::yield();
	} else hal::unblock_interruptions();
}

void IPC::receive(Message *message) {
	hal::block_interruptions();
	Thread *target = Scheduler::getCurrentThread();

	if (target->hasMessages()) {
		target->getNextMessage(*message);
		hal::unblock_interruptions();
		return;
	}

	target->setState(ThreadState::WAITING_MSG);
	hal::unblock_interruptions();
	Thread::yield();

	hal::block_interruptions();
	target->getNextMessage(*message);
	hal::unblock_interruptions();
}

} // namespace cock::core::task
