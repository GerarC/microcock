#include <cock/core/memory/vmm.hpp>
#include <cock/core/security/capability_transfer.hpp>
#include <cock/core/syscall/ipc/endpoint_route.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/reply_table.hpp>
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
using core::security::CapabilityNodeId;
using core::security::CapabilityType;
using core::security::NO_CAPABILITY_NODE;
using core::task::IPC;
using core::task::Process;
using core::task::reply_table;
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
		type == MessageType::PROCESS_EXITED || type == MessageType::SIGNAL)
		return false;
	if (type == MessageType::CAPABILITY_GRANTED)
		return thread.getProcess()->getCapabilities().holdsGrantAuthority();
	return true;
}

static bool user_message_ok(const Thread &thread, uintptr_t ptr, bool write) {
	return VirtualMemoryManager::isUserRangeValid(VirtAddr(ptr), sizeof(Message), write,
												  thread.getUserSpaceLimit());
}

static CapabilityNodeId attach_capability(const Thread &sender, Process &receiver,
										  Message &message) {
	if (&receiver == sender.getProcess() || receiver.isKernel()) return NO_CAPABILITY_NODE;

	uint32_t type = message.payload.args.arg0;
	uint32_t flags = message.payload.args.arg3;
	if (type < static_cast<uint32_t>(abi::FIRST_CAPABILITY_TYPE) ||
		type > static_cast<uint32_t>(abi::HIGHEST_CAPABILITY_TYPE))
		return NO_CAPABILITY_NODE;
	if (flags & ~abi::ALL_GRANT_FLAGS) return NO_CAPABILITY_NODE;

	CapabilityType capability_type = static_cast<CapabilityType>(type);
	if (!receiver.acceptsCapability(capability_type)) return NO_CAPABILITY_NODE;

	CapabilityNodeId node = security::delegate_capability(
		*sender.getProcess(), receiver, capability_type, message.payload.args.arg1,
		message.payload.args.arg2, (flags & abi::GRANT_DELEGATE) != 0,
		(flags & abi::GRANT_MINT) != 0,
		capability_type == CapabilityType::ENDPOINT ? message.payload.args.arg5
													: abi::endpoint::NO_BADGE);
	message.payload.args.arg4 = node;
	return node;
}

static uintptr_t queue_message(Thread &current, Message &message, Thread &target) {
	CapabilityNodeId node = NO_CAPABILITY_NODE;
	if (message.type == MessageType::CAPABILITY_TRANSFER) {
		node = attach_capability(current, *target.getProcess(), message);
		if (node == NO_CAPABILITY_NODE) return FAILURE;
	}

	if (IPC::deliver(target.getId(), message)) return node == NO_CAPABILITY_NODE ? SUCCESS : node;
	if (node != NO_CAPABILITY_NODE) security::withdraw_capability(*target.getProcess(), node);
	return FAILURE;
}

static uintptr_t post_message(Thread &current, Message &message, const EndpointRoute &route) {
	CapabilityNodeId node = NO_CAPABILITY_NODE;
	if (message.type == MessageType::CAPABILITY_TRANSFER) {
		if (route.target == nullptr) return FAILURE;
		node = attach_capability(current, *route.target->getProcess(), message);
		if (node == NO_CAPABILITY_NODE) return FAILURE;
	}

	if (IPC::post(route.endpoint, message)) return node == NO_CAPABILITY_NODE ? SUCCESS : node;
	if (node != NO_CAPABILITY_NODE) security::withdraw_capability(*route.target->getProcess(), node);
	return FAILURE;
}

uintptr_t sys_ipc_send(uintptr_t to_pid, uintptr_t msg_ptr, uintptr_t,
					   uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !user_message_ok(*current, msg_ptr, false)) return FAILURE;

	Message message;
	memcpy(&message, reinterpret_cast<const void *>(msg_ptr), sizeof(message));
	if (!sender_may_use(*current, message.type)) return FAILURE;
	message.badge = abi::endpoint::NO_BADGE;
	message.reply = abi::endpoint::NO_REPLY;

	Thread *target = TaskManager::getThreadByPID(static_cast<uint32_t>(to_pid));
	if (!target || target->getProcess() != current->getProcess()) return FAILURE;
	return queue_message(*current, message, *target);
}

uintptr_t sys_ep_send(uintptr_t handle, uintptr_t msg_ptr, uintptr_t flags,
					  uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !user_message_ok(*current, msg_ptr, false)) return FAILURE;
	if (flags & ~static_cast<uintptr_t>(abi::endpoint::ALL_SEND_FLAGS)) return FAILURE;

	Message message;
	memcpy(&message, reinterpret_cast<const void *>(msg_ptr), sizeof(message));
	if (!sender_may_use(*current, message.type)) return FAILURE;

	EndpointRoute route = resolveHandle(*current, handle);
	if (route.endpoint == abi::endpoint::NO_ENDPOINT) return FAILURE;
	message.badge = route.badge;
	message.reply = abi::endpoint::NO_REPLY;

	uint32_t token = abi::endpoint::NO_REPLY;
	if (flags & abi::endpoint::SEND_EXPECT_REPLY) {
		token = reply_table.openQueued(current->getId(), current->getProcess()->getId());
		if (token == abi::endpoint::NO_REPLY) return FAILURE;
		message.reply = token;
	}

	uintptr_t result = post_message(*current, message, route);
	if (result == FAILURE && token != abi::endpoint::NO_REPLY) reply_table.close(token);
	return result;
}

uintptr_t sys_ep_reply(uintptr_t token, uintptr_t msg_ptr, uintptr_t,
					   uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !user_message_ok(*current, msg_ptr, false)) return FAILURE;
	if (token > UINT32_MAX ||
		!reply_table.holds(static_cast<uint32_t>(token), current->getProcess()->getId()))
		return FAILURE;

	Message message;
	memcpy(&message, reinterpret_cast<const void *>(msg_ptr), sizeof(message));
	if (!sender_may_use(*current, message.type) ||
		message.type == MessageType::CAPABILITY_TRANSFER)
		return FAILURE;
	message.badge = abi::endpoint::NO_BADGE;
	message.reply = abi::endpoint::NO_REPLY;

	uint32_t asked = reply_table.requesterOf(static_cast<uint32_t>(token));
	Thread *requester = TaskManager::getThreadByPID(asked);
	if (requester == nullptr || requester->getState() == ThreadState::DEAD) {
		reply_table.close(static_cast<uint32_t>(token));
		return FAILURE;
	}
	if (!IPC::deliver(asked, message)) return FAILURE;
	reply_table.close(static_cast<uint32_t>(token));
	return SUCCESS;
}

uintptr_t sys_ep_forward(uintptr_t token, uintptr_t handle, uintptr_t msg_ptr,
						 uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !user_message_ok(*current, msg_ptr, false)) return FAILURE;
	uint32_t holder = current->getProcess()->getId();
	if (token > UINT32_MAX || !reply_table.holds(static_cast<uint32_t>(token), holder))
		return FAILURE;

	Message message;
	memcpy(&message, reinterpret_cast<const void *>(msg_ptr), sizeof(message));
	if (!sender_may_use(*current, message.type) ||
		message.type == MessageType::CAPABILITY_TRANSFER)
		return FAILURE;

	EndpointRoute route = resolveHandle(*current, handle);
	if (route.endpoint == abi::endpoint::NO_ENDPOINT) return FAILURE;
	message.badge = route.badge;
	message.reply = static_cast<uint32_t>(token);

	if (!reply_table.park(message.reply, holder)) return FAILURE;
	if (IPC::post(route.endpoint, message)) return SUCCESS;
	reply_table.claim(message.reply, holder);
	return FAILURE;
}

static bool take_next(Thread &thread, Message &message) {
	uint32_t line = 0;
	if (thread.takeNotification(line)) {
		message.target_pid = thread.getId();
		message.type = MessageType::IRQ_NOTIFICATION;
		message.payload.args.arg0 = line;
		return true;
	}
	task::ExitNotice notice;
	if (thread.takeExitNotice(notice)) {
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
