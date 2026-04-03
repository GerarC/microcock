#include "cock/core/cock.hpp"
#include "cock/core/task/ipc.hpp"
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/thread.hpp>
#include <cock/driver/serial.hpp>
#include <cock/driver/vga.hpp>
#include <string.h>
#include <stdint.h>

namespace cock::core::syscall {

using cock::driver::Serial;
using cock::driver::vga_instance;

uintptr_t sys_write(uintptr_t fd, uintptr_t buf_ptr, uintptr_t count, uintptr_t,
					uintptr_t, uintptr_t) {
	if (fd == 1 || fd == 2) {
		const char *user_buf = reinterpret_cast<const char *>(buf_ptr);

		Serial::write(user_buf, count);

		if (cock::vga_server_pid != 0) {
			size_t sent = 0;
			while (sent < count) {
				size_t chunk_size = (count - sent > 20) ? 20 : (count - sent);

				ipc::Message msg;
				msg.type = ipc::MessageType::VIDEO_WRITE_STR;
				msg.payload.args.arg0 = chunk_size;

				memcpy(&msg.payload.args.arg1, user_buf + sent, chunk_size);

				task::IPC::send(cock::vga_server_pid, msg);
				sent += chunk_size;
			}
		} else if (vga_instance && vga_instance->isEnabled())
			vga_instance->write(user_buf, count);

		return count;
	}
	return (uintptr_t)-1;
}

} // namespace cock::core::syscall
