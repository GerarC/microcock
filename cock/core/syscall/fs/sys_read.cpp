#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/utils/logger.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using core::task::Scheduler;
using core::task::Thread;
using core::task::ThreadResult;
using utils::Logger;

// TODO: FINISH SYS_READ
uintptr_t sys_read(uintptr_t fd, uintptr_t buf, uintptr_t count, uintptr_t,
				   uintptr_t, uintptr_t) {

	if (fd == 0) {
		char *user_buf = reinterpret_cast<char *>(buf);
		Logger::warn("sys_read: PID %d tried to read STDIN, but micrococks "
					 "lacks of TTY server!",
					 task::Scheduler::getCurrentThread()->getId());

		return 0;
	}
	return 1;
}

} // namespace cock::core::syscall
