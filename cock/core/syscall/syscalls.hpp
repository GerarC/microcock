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

/**
 * @brief Takes the next message or notification without blocking; returns 1 when there is none.
 */
uintptr_t sys_ipc_try_recv(uintptr_t msg_ptr, uintptr_t, uintptr_t, uintptr_t,
						   uintptr_t, uintptr_t);

uintptr_t sys_cap_grant(uintptr_t target_pid, uintptr_t type, uintptr_t first,
						uintptr_t count, uintptr_t, uintptr_t);

uintptr_t sys_irq_bind(uintptr_t line, uintptr_t, uintptr_t, uintptr_t,
					   uintptr_t, uintptr_t);

uintptr_t sys_irq_ack(uintptr_t line, uintptr_t, uintptr_t, uintptr_t,
					  uintptr_t, uintptr_t);

uintptr_t sys_proc_lookup(uintptr_t name_ptr, uintptr_t length, uintptr_t,
						  uintptr_t, uintptr_t, uintptr_t);


uintptr_t sys_shm_create(uintptr_t pages, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t);

uintptr_t sys_shm_share(uintptr_t id, uintptr_t target_pid, uintptr_t writable,
						uintptr_t, uintptr_t, uintptr_t);

uintptr_t sys_shm_map(uintptr_t id, uintptr_t writable, uintptr_t, uintptr_t,
					  uintptr_t, uintptr_t);

uintptr_t sys_shm_release(uintptr_t id, uintptr_t, uintptr_t, uintptr_t,
						  uintptr_t, uintptr_t);

uintptr_t sys_module_open(uintptr_t name_ptr, uintptr_t length, uintptr_t,
						  uintptr_t, uintptr_t, uintptr_t);

uintptr_t sys_module_index(uintptr_t name_ptr, uintptr_t length, uintptr_t,
						   uintptr_t, uintptr_t, uintptr_t);

uintptr_t sys_spawn(uintptr_t image_ptr, uintptr_t image_size, uintptr_t name_ptr,
					uintptr_t name_length, uintptr_t, uintptr_t);

uintptr_t sys_proc_alive(uintptr_t pid, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t);

constexpr uintptr_t MAX_TIMEOUT_MILLISECONDS = 0x7FFFFFFF;

/**
 * @brief Like sys_ipc_recv but gives up after the given milliseconds and
 * returns 1.
 */
uintptr_t sys_ipc_recv_timeout(uintptr_t msg_ptr, uintptr_t milliseconds,
							   uintptr_t, uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Blocks the calling thread for at least the given milliseconds.
 */
uintptr_t sys_sleep(uintptr_t milliseconds, uintptr_t, uintptr_t, uintptr_t,
					uintptr_t, uintptr_t);

/**
 * @brief Starts another thread of the calling process on a stack the caller
 * owns; returns the thread id.
 */
uintptr_t sys_thread_create(uintptr_t entry, uintptr_t stack_base,
							uintptr_t stack_size, uintptr_t, uintptr_t,
							uintptr_t);

/**
 * @brief Synchronous call: sends four words to a thread and blocks until it
 * replies; the answer comes back in the registers of the call.
 */
uintptr_t sys_ipc_call(uintptr_t target, uintptr_t word0, uintptr_t word1,
					   uintptr_t word2, uintptr_t word3, uintptr_t milliseconds);

/**
 * @brief Waits for the next synchronous call; the caller id and the four
 * words come back in the registers of the call.
 */
uintptr_t sys_ipc_accept(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t milliseconds);

/**
 * @brief Answers the call being served with four words.
 */
uintptr_t sys_ipc_reply(uintptr_t, uintptr_t word0, uintptr_t word1, uintptr_t word2,
						uintptr_t word3, uintptr_t);

/**
 * @brief Answers the call being served, if any, and waits for the next one.
 */
uintptr_t sys_ipc_reply_wait(uintptr_t, uintptr_t word0, uintptr_t word1,
							 uintptr_t word2, uintptr_t word3, uintptr_t milliseconds);

/**
 * @brief Detaches the call being served and returns its ticket, so the server can
 * accept other calls and answer this one later with sys_ipc_reply_to.
 */
uintptr_t sys_ipc_defer(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						uintptr_t);

/**
 * @brief Answers a detached call, named by its ticket, with four words.
 */
uintptr_t sys_ipc_reply_to(uintptr_t ticket, uintptr_t word0, uintptr_t word1,
						   uintptr_t word2, uintptr_t word3, uintptr_t);

uintptr_t sys_gettid(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
					 uintptr_t);

/**
 * @brief Blocks while the word at the address still holds the expected value.
 */
uintptr_t sys_futex_wait(uintptr_t address, uintptr_t expected, uintptr_t milliseconds,
						 uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Wakes up to count threads of the caller process waiting on the word.
 */
uintptr_t sys_futex_wake(uintptr_t address, uintptr_t count, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t);

} // namespace cock::core::syscall

#endif
