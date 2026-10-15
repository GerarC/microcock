#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/ipc/message.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::syscall {

using core::memory::VirtAddr;
using core::memory::VirtualMemoryManager;
using core::task::IPC;
using core::task::Scheduler;
using core::task::Thread;
using ipc::Message;

constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uintptr_t SUCCESS = 0;

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
	return IPC::deliver(static_cast<uint32_t>(to_pid), message) ? SUCCESS
																: FAILURE;
}

uintptr_t sys_ipc_recv(uintptr_t msg_ptr, uintptr_t, uintptr_t, uintptr_t,
					   uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !user_message_ok(*current, msg_ptr, true)) return FAILURE;

	Message message = {};
	IPC::receive(&message);
	memcpy(reinterpret_cast<void *>(msg_ptr), &message, sizeof(message));
	return SUCCESS;
}

} // namespace cock::core::syscall
