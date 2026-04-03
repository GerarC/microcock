#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/ipc.hpp>
#include <cock/ipc/message.hpp>
#include <stdint.h>

namespace cock::core::syscall {

uintptr_t sys_ipc_send(uintptr_t to_pid, uintptr_t msg_ptr, uintptr_t,
					   uintptr_t, uintptr_t, uintptr_t) {
	cock::ipc::Message *msg = reinterpret_cast<cock::ipc::Message *>(msg_ptr);
	cock::core::task::IPC::send(to_pid, *msg); 
	return 0;
}

uintptr_t sys_ipc_recv(uintptr_t msg_ptr, uintptr_t, uintptr_t, uintptr_t,
					   uintptr_t, uintptr_t) {
	cock::ipc::Message *msg = reinterpret_cast<cock::ipc::Message *>(msg_ptr);
	cock::core::task::IPC::receive(msg); 
	return 0;
}

} // namespace cock::core::syscall
