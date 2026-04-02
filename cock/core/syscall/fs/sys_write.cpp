#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/driver/vga.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using cock::driver::vga_instance;

uintptr_t sys_write(uintptr_t fd, uintptr_t buf_ptr, uintptr_t count, uintptr_t,
					uintptr_t, uintptr_t) {
	if (fd == 1 || fd == 2) {
		const char *user_buf = reinterpret_cast<const char *>(buf_ptr);
		vga_instance->write(user_buf, count);
		return count;
	}
	return (uintptr_t)-1;
}

} // namespace cock::core::syscall
