#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/ipc_access.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/core/task/timeouts.hpp>
#include <cock/ipc/message.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::syscall {

using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::task::IPC;
using core::task::Scheduler;
using core::task::TaskManager;
using core::task::Thread;
using core::task::ThreadState;
using core::task::TimeoutPhase;
using core::task::Timeouts;
using ipc::Message;
using ipc::MessageType;

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;
constexpr uintptr_t NO_MESSAGE = 1;

static bool sender_may_use(const Thread &thread, MessageType type) {
	if (type == MessageType::IRQ_NOTIFICATION ||
		type == MessageType::PROCESS_EXITED)
		return false;
	if (type == MessageType::CAPABILITY_GRANTED)
		return thread.getProcess()->getCapabilities().holdsGrantAuthority();
	return true;
}

static bool user_message_ok(const Thread &thread, uintptr_t ptr, bool write) {
	return VirtualMemoryManager::isUserRangeValid(VirtAddr(ptr), sizeof(Message), write,
												  thread.getUserSpaceLimit());
}

uintptr_t sys_ipc_send(uintptr_t to_pid, uintptr_t msg_ptr, uintptr_t,
					   uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !user_message_ok(*current, msg_ptr, false)) return FAILURE;

	Message message;
	memcpy(&message, reinterpret_cast<const void *>(msg_ptr), sizeof(message));
	if (!sender_may_use(*current, message.type)) return FAILURE;

	Thread *target = TaskManager::getThreadByPID(static_cast<uint32_t>(to_pid));
	if (!target || !security::mayTalk(*current->getProcess(), *target->getProcess()))
		return FAILURE;
	return IPC::deliver(static_cast<uint32_t>(to_pid), message) ? SUCCESS
																: FAILURE;
}

static bool take_next(Thread &thread, Message &message) {
	uint32_t line = 0;
	if (thread.takeNotification(line)) {
		message.sender_pid = ipc::KERNEL_SENDER;
		message.target_pid = thread.getId();
		message.type = MessageType::IRQ_NOTIFICATION;
		message.payload.args.arg0 = line;
		return true;
	}
	task::ExitNotice notice;
	if (thread.takeExitNotice(notice)) {
		message.sender_pid = ipc::KERNEL_SENDER;
		message.target_pid = thread.getId();
		message.type = MessageType::PROCESS_EXITED;
		message.payload.args.arg0 = notice.child;
		message.payload.args.arg1 = notice.status;
		return true;
	}
	return thread.getNextMessage(message);
}

uintptr_t sys_ipc_try_recv(uintptr_t msg_ptr, uintptr_t, uintptr_t, uintptr_t,
						   uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !user_message_ok(*current, msg_ptr, true)) return FAILURE;

	Message message = {};
	if (!take_next(*current, message)) return NO_MESSAGE;

	memcpy(reinterpret_cast<void *>(msg_ptr), &message, sizeof(message));
	return SUCCESS;
}

uintptr_t sys_ipc_recv(uintptr_t msg_ptr, uintptr_t, uintptr_t, uintptr_t,
					   uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !user_message_ok(*current, msg_ptr, true)) return FAILURE;

	Message message = {};
	if (!take_next(*current, message)) {
		current->setState(ThreadState::WAITING_MSG);
		Scheduler::requestReschedule();
		Scheduler::requestRestart();
		return SUCCESS;
	}

	memcpy(reinterpret_cast<void *>(msg_ptr), &message, sizeof(message));
	return SUCCESS;
}

uintptr_t sys_ipc_recv_timeout(uintptr_t msg_ptr, uintptr_t milliseconds,
							   uintptr_t, uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !user_message_ok(*current, msg_ptr, true)) return FAILURE;
	if (milliseconds > MAX_TIMEOUT_MILLISECONDS) return FAILURE;

	Message message = {};
	if (take_next(*current, message)) {
		Timeouts::cancel(current);
		memcpy(reinterpret_cast<void *>(msg_ptr), &message, sizeof(message));
		return SUCCESS;
	}

	TimeoutPhase phase = Timeouts::wait(
		*current, static_cast<uint32_t>(milliseconds), ThreadState::WAITING_MSG);
	return phase == TimeoutPhase::TIMED_OUT ? NO_MESSAGE : SUCCESS;
}

} // namespace cock::core::syscall
