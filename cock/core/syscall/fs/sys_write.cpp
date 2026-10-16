#include <cock/core/cock.hpp>
#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/ipc/message.hpp>
#include <hal/console.hpp>
#include <stdint.h>
#include <string.h>

namespace cock::core::syscall {

using core::task::IPC;
using core::task::Scheduler;
using core::task::TaskManager;
using core::task::ThreadState;
using core::task::Thread;
using ipc::Message;
using ipc::MessageType;
using memory::VirtAddr;
using memory::VirtualMemoryManager;

constexpr uintptr_t STDOUT_FD = 1;
constexpr uintptr_t STDERR_FD = 2;
constexpr uintptr_t MAX_WRITE_SIZE = 0x80;
constexpr size_t VIDEO_CHUNK_SIZE = 20;
constexpr uint32_t WRITE_RETRY_LIMIT = 1000;
constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);

static size_t forward_to_video_server(const char *data, size_t count) {
	size_t sent = 0;
	while (sent < count) {
		size_t chunk_size = count - sent;
		if (chunk_size > VIDEO_CHUNK_SIZE) chunk_size = VIDEO_CHUNK_SIZE;

		Message msg = {};
		msg.type = MessageType::VIDEO_WRITE_STR;
		msg.payload.args.arg0 = static_cast<uint32_t>(chunk_size);
		memcpy(&msg.payload.args.arg1, data + sent, chunk_size);

		if (!IPC::deliver(cock::vga_server_pid, msg)) break;
		sent += chunk_size;
	}
	return sent;
}

uintptr_t sys_write(uintptr_t fd, uintptr_t buf_ptr, uintptr_t count, uintptr_t,
					uintptr_t, uintptr_t) {
	if (fd != STDOUT_FD && fd != STDERR_FD) return FAILURE;
	if (count > MAX_WRITE_SIZE) count = MAX_WRITE_SIZE;

	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;
	if (!VirtualMemoryManager::isUserRangeValid(VirtAddr(buf_ptr), count, false,
												current->getUserSpaceLimit()))
		return FAILURE;

	const char *user_buf = reinterpret_cast<const char *>(buf_ptr);
	if (cock::vga_server_pid == 0) {
		hal::console_write(user_buf, count);
		return count;
	}

	size_t accepted = forward_to_video_server(user_buf, count);
	if (accepted != 0) current->resetWriteRetries();
	if (accepted == 0 && count != 0) {
		Thread *server = TaskManager::getThreadByPID(cock::vga_server_pid);
		if (server && server->getState() != ThreadState::DEAD) {
			if (current->countWriteRetry() > WRITE_RETRY_LIMIT) {
				current->resetWriteRetries();
				hal::console_write(user_buf, count);
				return count;
			}
			Scheduler::requestReschedule();
			Scheduler::requestRestart();
			return 0;
		}

		cock::vga_server_pid = 0;
		hal::console_attach();
		hal::console_write(user_buf, count);
		return count;
	}

	hal::console_write(user_buf, accepted);
	return accepted;
}

} // namespace cock::core::syscall
