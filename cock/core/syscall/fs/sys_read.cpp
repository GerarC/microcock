#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::task::Scheduler;
using core::task::Thread;
using memory::VirtualMemoryManager;
using utils::Logger;

constexpr uintptr_t STDIN_FD = 0;

// TODO: FINISH SYS_READ
uintptr_t sys_read(uintptr_t fd, uintptr_t buf, uintptr_t count, uintptr_t,
				   uintptr_t, uintptr_t) {
	if (fd != STDIN_FD) return (uintptr_t)-1;

	Thread *current = Scheduler::getCurrentThread();
	if (!current) return (uintptr_t)-1;
	if (!VirtualMemoryManager::isUserRangeValid(buf, count, true,
												current->getUserSpaceLimit()))
		return (uintptr_t)-1;

	Logger::warn("sys_read: PID %d tried to read STDIN, but micrococks "
				 "lacks of TTY server!",
				 current->getId());

	return 0;
}

} // namespace cock::core::syscall
