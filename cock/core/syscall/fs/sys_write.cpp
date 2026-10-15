#include <cock/core/memory/vmm.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/driver/vga.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using cock::driver::vga_instance;
using core::task::Scheduler;
using core::task::Thread;
using memory::VirtualMemoryManager;

constexpr uintptr_t STDOUT_FD = 1;
constexpr uintptr_t STDERR_FD = 2;
constexpr uintptr_t MAX_WRITE_SIZE = 0x10000;

uintptr_t sys_write(uintptr_t fd, uintptr_t buf_ptr, uintptr_t count, uintptr_t,
					uintptr_t, uintptr_t) {
	if (fd != STDOUT_FD && fd != STDERR_FD) return (uintptr_t)-1;
	if (count > MAX_WRITE_SIZE) count = MAX_WRITE_SIZE;

	Thread *current = Scheduler::getCurrentThread();
	if (!current) return (uintptr_t)-1;
	if (!VirtualMemoryManager::isUserRangeValid(buf_ptr, count, false,
												current->getUserSpaceLimit()))
		return (uintptr_t)-1;

	const char *user_buf = reinterpret_cast<const char *>(buf_ptr);
	vga_instance->write(user_buf, count);
	return count;
}

} // namespace cock::core::syscall
