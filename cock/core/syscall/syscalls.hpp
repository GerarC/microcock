#ifndef CORE_SYSCALL_MANAGER_HPP
#define CORE_SYSCALL_MANAGER_HPP

#include <stdint.h>

namespace cock::core::syscall {

typedef uintptr_t (*SystemCall)(uintptr_t, uintptr_t, uintptr_t, uintptr_t,
								uintptr_t, uintptr_t);


// FS
uintptr_t sys_write(uintptr_t fd, uintptr_t buf_ptr, uintptr_t count, uintptr_t, uintptr_t, uintptr_t);
uintptr_t sys_read(uintptr_t fd, uintptr_t buf, uintptr_t count, uintptr_t, uintptr_t, uintptr_t);

// HW
uintptr_t sys_request_resource(uintptr_t resource_id, uintptr_t size, uintptr_t, uintptr_t, uintptr_t, uintptr_t);

// Proc
uintptr_t sys_getpid(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
uintptr_t sys_sched_yield(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
uintptr_t sys_exit(uintptr_t status, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);

// MM
uintptr_t sys_mmap(uintptr_t addr, uintptr_t length, uintptr_t prot, uintptr_t flags, uintptr_t fd, uintptr_t offset);
uintptr_t sys_munmap(uintptr_t addr, uintptr_t length, uintptr_t, uintptr_t, uintptr_t, uintptr_t);


// IPC
uintptr_t sys_ipc_send(uintptr_t to_pid, uintptr_t msg_ptr, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
uintptr_t sys_ipc_recv(uintptr_t msg_ptr, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);


} // namespace cock::core::syscall

#endif
