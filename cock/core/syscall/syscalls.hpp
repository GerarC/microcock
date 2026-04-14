#ifndef CORE_SYSCALL_MANAGER_HPP
#define CORE_SYSCALL_MANAGER_HPP

#include <stdint.h>

namespace cock::core::syscall {

/**
 * @brief Function pointer type defining the standard signature for all system
 * calls.
 */
typedef uintptr_t (*SystemCall)(uintptr_t, uintptr_t, uintptr_t, uintptr_t,
								uintptr_t, uintptr_t);

// ==== File System & I/O ====
/**
 * @brief Writes data to a specified file descriptor (e.g., STDOUT mapped to
 * Serial/VGA).
 */
uintptr_t sys_write(uintptr_t fd, uintptr_t buf_ptr, uintptr_t count, uintptr_t,
					uintptr_t, uintptr_t);

/**
 * @brief Reads data from a specified file descriptor.
 */
uintptr_t sys_read(uintptr_t fd, uintptr_t buf, uintptr_t count, uintptr_t,
				   uintptr_t, uintptr_t);

// ==== Hardware & Resources ====
/**
 * @brief Requests access to a specific hardware resource (like I/O ports via
 * IOPM modification).
 */
uintptr_t sys_request_resource(uintptr_t resource_id, uintptr_t size, uintptr_t,
							   uintptr_t, uintptr_t, uintptr_t);

// ==== Process Management ====
/**
 * @brief Retrieves the Process ID (PID) of the currently executing thread.
 */
uintptr_t sys_getpid(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
					 uintptr_t);

/**
 * @brief Voluntarily yields the CPU to the next thread in the scheduler's ready
 * queue.
 */
uintptr_t sys_sched_yield(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						  uintptr_t);

/**
 * @brief Terminates the currently executing thread and notifies the task
 * manager for cleanup.
 */
uintptr_t sys_exit(uintptr_t status, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
				   uintptr_t);

// ==== Memory Management ====
/**
 * @brief Allocates dynamic virtual memory pages for the calling process
 * (Supports Anonymous mapping).
 */
uintptr_t sys_mmap(uintptr_t addr, uintptr_t length, uintptr_t prot,
				   uintptr_t flags, uintptr_t fd, uintptr_t offset);

/**
 * @brief Frees dynamically allocated memory pages.
 */
uintptr_t sys_munmap(uintptr_t addr, uintptr_t length, uintptr_t, uintptr_t,
					 uintptr_t, uintptr_t);

// ==== Inter-Process Communication ====
/**
 * @brief Sends an asynchronous IPC message to a specified target Process ID.
 */
uintptr_t sys_ipc_send(uintptr_t to_pid, uintptr_t msg_ptr, uintptr_t,
					   uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Blocks the thread and receives the next IPC message from its inbox.
 */
uintptr_t sys_ipc_recv(uintptr_t msg_ptr, uintptr_t, uintptr_t, uintptr_t,
					   uintptr_t, uintptr_t);

} // namespace cock::core::syscall

#endif
