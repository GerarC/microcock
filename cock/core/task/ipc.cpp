#include "cock/core/task/scheduler.hpp"
#include "cock/core/task/task_manager.hpp"
#include "cock/core/task/thread.hpp"
#include <cock/core/task/ipc.hpp>

namespace cock::core::task {

void IPC::send(uint32_t to_pid, Message message) {
	Thread *target = TaskManager::getThreadByPID(to_pid);
	Thread *sender = Scheduler::getCurrentThread();

	if (!target || target->getState() == ThreadState::DEAD) return;
	message.sender_pid = sender->getId();
	message.target_pid = target->getId();
	target->receiveMessage(message);

	if (target->getState() == ThreadState::WAITING_MSG) {
		target->setState(ThreadState::READY);
		Scheduler::addThread(target);
	}
}

void IPC::receive(Message *message) {
	Thread *target = Scheduler::getCurrentThread();
	while (!target->hasMessages()) {
		target->setState(ThreadState::WAITING_MSG);
		Thread::yield();
	}
    target->getNextMessage(*message);
}

} // namespace cock::core::task
