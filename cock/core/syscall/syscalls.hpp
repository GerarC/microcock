/** @file
 * @brief Declarations of every system call handler of the kernel.
 *
 * All handlers share one signature: six untrusted register values in, one
 * register value out. They run in the context of the calling thread, with the
 * user address space active. Handlers never trust a pointer or a number: every
 * user range goes through VirtualMemoryManager::isUserRangeValid and every id
 * is looked up before use. Unless stated otherwise a failure returns all ones
 * (the value of minus one) and success returns 0 or a meaningful value. Unused
 * parameters are ignored. A handler may ask the scheduler to reschedule or to
 * restart the call, which is how blocking is implemented.
 */
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
 * @param fd Descriptor, 1 (standard output) or 2 (standard error). Both go to
 * the same console.
 * @param buf_ptr User pointer to the bytes to write.
 * @param count Number of bytes. Clamped to a small maximum per call.
 * @return Number of bytes accepted, which may be less than requested. All
 * ones for a bad descriptor or an invalid user buffer. Returns 0 and asks to
 * restart the call when the video server is alive but its inbox is full.
 * @note Without a video server the bytes go straight to the kernel console.
 * If the video server died the kernel console is taken back.
 */
uintptr_t sys_write(uintptr_t fd, uintptr_t buf_ptr, uintptr_t count, uintptr_t,
					uintptr_t, uintptr_t);

/**
 * @brief Reads data from a specified file descriptor.
 * @param fd Descriptor, only 0 (standard input) is accepted.
 * @param buf User buffer to fill, validated as writable.
 * @param count Size of the buffer.
 * @return 0, since the kernel has no input. All ones for another descriptor
 * or an invalid buffer. Input is served by the console server in Ring 3.
 */
uintptr_t sys_read(uintptr_t fd, uintptr_t buf, uintptr_t count, uintptr_t,
				   uintptr_t, uintptr_t);

// ==== Hardware & Resources ====
/**
 * @brief Requests access to a specific hardware resource (like I/O ports via
 * IOPM modification).
 * @param resource_id First resource, a port number on x86.
 * @param size Number of consecutive resources.
 * @return 0 on success, all ones when the process holds no I/O port
 * capability covering the whole range or the HAL refuses it (empty or
 * out of range, kernel owned ports, no memory).
 */
uintptr_t sys_request_resource(uintptr_t resource_id, uintptr_t size, uintptr_t,
							   uintptr_t, uintptr_t, uintptr_t);

// ==== Process Management ====
/**
 * @brief Retrieves the Process ID (PID) of the currently executing thread.
 * @return The process id, or all ones when there is no current thread.
 */
uintptr_t sys_getpid(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
					 uintptr_t);

/**
 * @brief Voluntarily yields the CPU to the next thread in the scheduler's ready
 * queue.
 * @return Always 0.
 */
uintptr_t sys_sched_yield(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						  uintptr_t);

/**
 * @brief Terminates the currently executing thread and notifies the task
 * manager for cleanup.
 * @param status Exit status stored in the process, truncated to 32 bits.
 * @return 0. The caller never runs again.
 */
uintptr_t sys_exit(uintptr_t status, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
				   uintptr_t);

// ==== Memory Management ====
/**
 * @brief Allocates dynamic virtual memory pages for the calling process
 * (Supports Anonymous mapping).
 * @param addr Fixed virtual address, used only with the physical mapping flag.
 * @param length Size in bytes, rounded up to pages, must not be zero.
 * @param prot Ignored. Anonymous memory is always read and write.
 * @param flags Anonymous flag: zero filled private pages the kernel places.
 * Physical flag: maps device memory at a fixed address, needs a physical
 * memory capability covering the range and the range must not be RAM.
 * @param fd Ignored.
 * @param offset Physical address for the physical mapping flag.
 * @return The start address of the mapping, or all ones on failure: no flag,
 * size zero, quota or region slots exhausted, no free range, out of memory,
 * missing capability or a fixed address outside the user image range.
 */
uintptr_t sys_mmap(uintptr_t addr, uintptr_t length, uintptr_t prot,
				   uintptr_t flags, uintptr_t fd, uintptr_t offset);

/**
 * @brief Frees dynamically allocated memory pages.
 * @param addr Page aligned start of a region returned by mmap or shm map.
 * @param length Size in bytes, rounded up to pages.
 * @return 0 on success, all ones when the arguments do not describe a region
 * of the caller. A shared mapping also drops the mapping count of its object.
 */
uintptr_t sys_munmap(uintptr_t addr, uintptr_t length, uintptr_t, uintptr_t,
					 uintptr_t, uintptr_t);

/**
 * @brief Handler of every system call number that has no implementation: the
 * Linux numbers the kernel does not serve (listed in `syscall_nums.hpp`), the
 * numbers with no name and the numbers past the table.
 * @return All ones, the failure value of every system call. It touches nothing,
 * so any arguments are safe; the dispatcher logs the number at trace level.
 */
uintptr_t sys_not_implemented(uintptr_t, uintptr_t, uintptr_t, uintptr_t,
							  uintptr_t, uintptr_t);

// ==== Inter-Process Communication ====
/**
 * @brief Sends an asynchronous IPC message to a specified target Process ID.
 * @param to_pid Destination process id.
 * @param msg_ptr User pointer to a Message, copied before use.
 * @return 0 when queued, all ones when the buffer is invalid, the message
 * type is reserved to the kernel (IRQ notification, process exit), a
 * capability grant message is sent by a process without grant authority, or
 * the target is unknown, dead or has a full inbox.
 *
 * A `CAPABILITY_TRANSFER` message also gives the receiving process a copy of
 * part of a capability of the sender (arg0 type, arg1 first, arg2 count, arg3
 * flags: `GRANT_DELEGATE`, `GRANT_MINT`, both or none, arg5 the badge of an
 * `ENDPOINT` capability; for any other type arg5 is free). The copy is made before the message is queued
 * and undone if queuing fails. On success the call returns the node id of the
 * copy (above 0 and below all ones) instead of 0, and the receiver finds the
 * same id in arg4. It fails, with nothing given, when the sender and the
 * receiver may not talk, the receiver is the sender or a kernel process, the
 * type or flags are invalid, the receiver did not accept the type with
 * `SYS_CAP_ACCEPT`, the sender holds no grantable capability that covers the
 * range, or the table of the receiver or the derivation tree is full.
 */
uintptr_t sys_ipc_send(uintptr_t to_pid, uintptr_t msg_ptr, uintptr_t,
					   uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Sends an asynchronous message through a handle on an endpoint.
 * @param handle Number of an `ENDPOINT` capability of the caller (the node id that
 * a grant returned to the giver and a capability message put in arg4 for the
 * receiver). Holding it is the whole permission: no service label is compared.
 * @param msg_ptr User pointer to a Message, copied before use.
 * @return As `sys_ipc_send`, and all ones when the handle names no endpoint
 * capability of the caller, the endpoint is unbound or the process bound to it is
 * gone. The kernel writes the badge of the handle in `Message::badge`, whatever the
 * sender put there. A `CAPABILITY_TRANSFER` message works as in `sys_ipc_send`.
 */
uintptr_t sys_ep_send(uintptr_t handle, uintptr_t msg_ptr, uintptr_t,
					  uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Calls the process bound to an endpoint, with four words in registers.
 * @param handle Number of an `ENDPOINT` capability of the caller.
 * @param word0 First request word.
 * @param word1 Second request word.
 * @param word2 Third request word.
 * @param word3 Fourth request word.
 * @param milliseconds Time limit as in `sys_ipc_call`.
 * @return As `sys_ipc_call`. The server finds the badge of the handle in the `peer`
 * register of its accept, where a call by thread id puts the id of the caller.
 */
uintptr_t sys_ep_call(uintptr_t handle, uintptr_t word0, uintptr_t word1,
					  uintptr_t word2, uintptr_t word3, uintptr_t milliseconds);

/**
 * @brief Creates an endpoint and gives the caller the capability that owns it.
 * @param handle_ptr User pointer to a 32 bit value that receives the handle of the
 * owner capability (grant, mint and bind rights).
 * @return The endpoint id (from 1), or all ones when the caller holds no grant
 * authority, the pointer is invalid, the table of endpoints, the derivation tree or
 * the table of capabilities of the caller is full; nothing is created then.
 */
uintptr_t sys_ep_create(uintptr_t handle_ptr, uintptr_t, uintptr_t, uintptr_t,
						uintptr_t, uintptr_t);

/**
 * @brief Binds an endpoint to a process, which then receives what is sent to it.
 * @param endpoint Endpoint id. The caller needs an `ENDPOINT` capability with the bind
 * right that covers it.
 * @param pid Id of the process, which must be alive and not a kernel process. A
 * previous owner stops receiving.
 * @return 0 on success, all ones otherwise.
 */
uintptr_t sys_ep_bind(uintptr_t endpoint, uintptr_t pid, uintptr_t, uintptr_t,
					  uintptr_t, uintptr_t);

/**
 * @brief Blocks the thread and receives the next IPC message from its inbox.
 * @param msg_ptr User pointer where the Message is stored, validated as
 * writable.
 * @return 0, or all ones for an invalid buffer. With nothing pending the
 * thread waits and the call is restarted when a message arrives. A pending
 * IRQ notification is delivered before queued messages.
 */
uintptr_t sys_ipc_recv(uintptr_t msg_ptr, uintptr_t, uintptr_t, uintptr_t,
					   uintptr_t, uintptr_t);

/**
 * @brief Takes the next message or notification without blocking; returns 1 when there is none.
 * @param msg_ptr User pointer where the Message is stored, validated as
 * writable.
 * @return 0 when a message was stored, 1 when none is pending, all ones for an
 * invalid buffer.
 */
uintptr_t sys_ipc_try_recv(uintptr_t msg_ptr, uintptr_t, uintptr_t, uintptr_t,
						   uintptr_t, uintptr_t);

/**
 * @brief Grants a capability to another process.
 * @param target_pid Receiving process id. Kernel processes and stale ids are
 * refused.
 * @param type Capability type, between the first valid type and the highest.
 * @param first Start of the range (port, line, address or module index).
 * @param count Length of the range.
 * @param flags Zero, or `GRANT_DELEGATE` to let the target grant the copy on, or
 * `GRANT_MINT` (`ENDPOINT` only, and only if the caller holds the right) to let it
 * choose badges, or both.
 * @param badge `ENDPOINT` only: the badge of the copy. Only a caller that holds the mint
 * right may pass a value other than zero; any other caller's copy keeps the badge of its
 * source.
 * @return The id of the node of the derivation tree that records the grant
 * (above 0 and below all ones). All ones when the type or the flags are invalid, the
 * caller does not hold a grantable capability that covers the whole range, a mint
 * right or a badge is asked for by a caller that has no mint right, the target
 * is invalid, the target capability table is full or the derivation tree is
 * full. The copy carries the grant flag only with `GRANT_DELEGATE`.
 */
uintptr_t sys_cap_grant(uintptr_t target_pid, uintptr_t type, uintptr_t first,
						uintptr_t count, uintptr_t flags, uintptr_t badge);

/**
 * @brief Revokes a delegated capability and everything derived from it.
 * @param node Id of the node, as `sys_cap_grant` returned it.
 * @return 0 on success. All ones when the id is zero, above the highest id or
 * unknown, or when the caller holds no ancestor of the node (a root node, or a
 * node the caller holds itself, cannot be revoked by the caller). See
 * `revoke_capability` for what is undone.
 */
uintptr_t sys_cap_revoke(uintptr_t node, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t);

/**
 * @brief Chooses which capability types a message may give the calling process.
 * @param mask One bit per capability type (`abi::capabilityBit`); replaces the
 * previous mask. A process accepts nothing until it calls this.
 * @return 0 on success. All ones when the mask has a bit outside
 * `abi::ALL_CAPABILITY_TYPES`.
 */
uintptr_t sys_cap_accept(uintptr_t mask, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t);

/**
 * @brief Gives a process its service label, once.
 * @param target_pid Process id of a live user process; a thread id or a kernel
 * process is refused.
 * @param label New label, not `NO_SERVICE`.
 * @return 0 on success. All ones when the caller has no grant authority, the
 * target is invalid, the label is `NO_SERVICE` or the target already has a label.
 */
uintptr_t sys_service_set(uintptr_t target_pid, uintptr_t label, uintptr_t,
						  uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Binds a hardware interrupt line to the calling thread.
 * @param line Interrupt line.
 * @return 0 on success. All ones when the process lacks an IRQ capability for
 * the line, the line is reserved or it is already owned.
 */
uintptr_t sys_irq_bind(uintptr_t line, uintptr_t, uintptr_t, uintptr_t,
					   uintptr_t, uintptr_t);

/**
 * @brief Unmasks a line the calling thread owns, after servicing the device.
 * @param line Interrupt line.
 * @return 0 on success, all ones when the caller is not the owner.
 */
uintptr_t sys_irq_ack(uintptr_t line, uintptr_t, uintptr_t, uintptr_t,
					  uintptr_t, uintptr_t);

/**
 * @brief Gives the process a thread belongs to.
 * @param tid Id of a thread; the id of a process names its first thread.
 * @return The process id (the id of its first thread), or all ones when the id is
 * unknown, the thread is dead or it belongs to the kernel.
 */
uintptr_t sys_thread_process(uintptr_t tid, uintptr_t, uintptr_t, uintptr_t,
							 uintptr_t, uintptr_t);

/**
 * @brief Finds the process that the kernel started from a boot module, by module name.
 * @param name_ptr User pointer to the name bytes, not null terminated.
 * @param length Name length, from 1 up to the maximum name length minus one.
 * @return The id of the live process started from the boot module with that
 * name, or all ones when the caller holds no grant authority, the arguments are
 * invalid or no boot module process has the name. Processes started with the
 * `SYS_PROC_*` calls and the kernel process are never found.
 */
uintptr_t sys_module_process(uintptr_t name_ptr, uintptr_t length, uintptr_t,
							 uintptr_t, uintptr_t, uintptr_t);


/**
 * @brief Creates a shared memory object owned by the caller.
 * @param pages Size in pages, from 1 to the object limit.
 * @return The object id, or all ones when the size is invalid, the process is
 * over its page quota, the table is full or frames run out.
 */
uintptr_t sys_shm_create(uintptr_t pages, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t);

/**
 * @brief Gives another process access to a shared memory object.
 * @param id Object id, the caller must hold it.
 * @param target_pid Receiving process id. Kernel and stale ids are refused.
 * @param writable Non zero to grant write access, which the caller must have.
 * @return 0 on success, all ones on any failure.
 */
uintptr_t sys_shm_share(uintptr_t id, uintptr_t target_pid, uintptr_t writable,
						uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Maps a shared memory object into the caller address space.
 * @param id Object id, the caller must hold it.
 * @param writable Non zero for a writable mapping, which needs write access.
 * @return The address of the mapping, or all ones when access is denied, the
 * caller has no free region slot, no free range is left or a page cannot be
 * mapped.
 */
uintptr_t sys_shm_map(uintptr_t id, uintptr_t writable, uintptr_t, uintptr_t,
					  uintptr_t, uintptr_t);

/**
 * @brief Drops the caller access to a shared memory object.
 * @param id Object id.
 * @return 0 when the caller was a holder, all ones otherwise. Existing
 * mappings stay until unmapped.
 */
uintptr_t sys_shm_release(uintptr_t id, uintptr_t, uintptr_t, uintptr_t,
						  uintptr_t, uintptr_t);

/**
 * @brief Opens a boot data module by name as a read only shared object.
 * @param name_ptr User pointer to the name bytes, not null terminated.
 * @param length Name length, from 1 up to the name buffer size minus one.
 * @return The shared object id, or all ones when the name is unknown, the
 * process holds no boot module capability for that module or the buffer is
 * invalid.
 */
uintptr_t sys_module_open(uintptr_t name_ptr, uintptr_t length, uintptr_t,
						  uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Translates a boot module name to its index in the boot list.
 * @param name_ptr User pointer to the name bytes, not null terminated.
 * @param length Name length, from 1 up to the name buffer size minus one.
 * @return The module index, or all ones. Only a process that holds grant
 * authority may ask, since the index names the capability to grant.
 */
uintptr_t sys_module_index(uintptr_t name_ptr, uintptr_t length, uintptr_t,
						   uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Creates a process with an empty address space and no thread, the first
 * step of starting a program from a loader in user space.
 * @param name_ptr User pointer to the process name.
 * @param name_length Name length, from 1 up to the name limit minus one.
 * @param flags Spawn flags. The notify on exit flag makes the kernel send the
 * creator a message when the process exits, once it started.
 * @param rights_ptr Zero, or a user pointer to an `abi::service::SpawnRights`: the
 * label of the new process and the IPC labels it may talk to. They are checked
 * here and again, and set, when the process starts. Each IPC label needs a
 * grantable IPC capability of the caller that covers it, and the derivation tree
 * needs room for one node per label.
 * @param user_limit First address the process may not use: page aligned, not
 * above the architecture limit and above the user stack.
 * @return The handle of the pending process, or all ones when the caller holds
 * no SPAWN capability, it already holds `MAX_PENDING_PER_PROCESS` pending
 * processes, the system holds `MAX_PENDING_PROCESSES`, a number or buffer is
 * invalid, no process id is free, the rights are invalid or memory runs out.
 */
uintptr_t sys_proc_create(uintptr_t name_ptr, uintptr_t name_length, uintptr_t flags,
						  uintptr_t rights_ptr, uintptr_t user_limit, uintptr_t);

/**
 * @brief Maps fresh zeroed writable pages into a pending process.
 * @param handle Handle from `sys_proc_create`, owned by the caller.
 * @param address Page aligned start, not in the first page.
 * @param pages Page count, non zero. The range must end below the user stack of
 * the process, be unmapped, and keep the total of the process under
 * `MAX_PENDING_IMAGE_BYTES`.
 * @return 0, or all ones for a handle the caller does not own, a bad range, an
 * overlap or when memory runs out. A failure in the middle can leave pages mapped;
 * the caller discards the process.
 */
uintptr_t sys_proc_map(uintptr_t handle, uintptr_t address, uintptr_t pages, uintptr_t,
					   uintptr_t, uintptr_t);

/**
 * @brief Copies bytes from the caller into mapped, writable pages of a pending
 * process.
 * @param handle Handle from `sys_proc_create`, owned by the caller.
 * @param address Destination in the pending process.
 * @param source Source in the caller, validated as readable.
 * @param length Byte count, non zero and at most `MAX_PENDING_IMAGE_BYTES`.
 * @return 0, or all ones for a bad handle, a source that is not readable, a
 * destination range that is not mapped and writable in the pending process, or an
 * overflow.
 */
uintptr_t sys_proc_write(uintptr_t handle, uintptr_t address, uintptr_t source,
						 uintptr_t length, uintptr_t, uintptr_t);

/**
 * @brief Makes mapped pages of a pending process read only, so code cannot be
 * changed by the program afterwards.
 * @param handle Handle from `sys_proc_create`, owned by the caller.
 * @param address Page aligned start.
 * @param pages Page count, non zero; every page must be mapped.
 * @return 0, or all ones for a bad handle or range.
 */
uintptr_t sys_proc_protect(uintptr_t handle, uintptr_t address, uintptr_t pages,
						   uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Starts a pending process: creates its first thread at the entry point,
 * sets its rights and lets it run.
 * @param handle Handle from `sys_proc_create`, owned by the caller; it stops
 * being valid on success.
 * @param entry Entry point, inside a mapped page below the user stack.
 * @return The id of the first thread, which is the process id, or all ones when
 * the handle is not the caller's, the entry is invalid, no process id is free, the
 * rights are no longer valid or the caller has too many children with an exit
 * notice. The process stays pending after a failure.
 */
uintptr_t sys_proc_start(uintptr_t handle, uintptr_t entry, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t);

/**
 * @brief Destroys a pending process and frees its memory.
 * @param handle Handle from `sys_proc_create`, owned by the caller.
 * @return 0, or all ones for a handle the caller does not own.
 */
uintptr_t sys_proc_discard(uintptr_t handle, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						   uintptr_t);

/**
 * @brief Tells whether the thread of a process id is alive.
 * @param pid Process id, generation included.
 * @return 0 when the thread exists and is not dead, all ones otherwise.
 */
uintptr_t sys_proc_alive(uintptr_t pid, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t);

/** @brief Largest finite timeout in milliseconds accepted by the timed calls. */
constexpr uintptr_t MAX_TIMEOUT_MILLISECONDS = 0x7FFFFFFF;

/**
 * @brief Like sys_ipc_recv but gives up after the given milliseconds and
 * returns 1.
 * @param msg_ptr User pointer where the Message is stored, validated as
 * writable.
 * @param milliseconds Time limit, at most MAX_TIMEOUT_MILLISECONDS.
 * @return 0 when a message was received, 1 on timeout, all ones for an
 * invalid buffer or time limit.
 */
uintptr_t sys_ipc_recv_timeout(uintptr_t msg_ptr, uintptr_t milliseconds,
							   uintptr_t, uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Blocks the calling thread for at least the given milliseconds.
 * @param milliseconds Duration, at most MAX_TIMEOUT_MILLISECONDS. Zero only
 * yields.
 * @return 0, or all ones when the duration is too large.
 */
uintptr_t sys_sleep(uintptr_t milliseconds, uintptr_t, uintptr_t, uintptr_t,
					uintptr_t, uintptr_t);

/**
 * @brief Starts another thread of the calling process on a stack the caller
 * owns; returns the thread id.
 * @param entry User address where the thread starts.
 * @param stack_base Lowest address of the stack, writable user memory.
 * @param stack_size Stack size in bytes, at least the ABI minimum.
 * @return The new thread id, or all ones when the caller is a kernel thread,
 * the stack is too small, entry or stack are invalid, or the thread cannot be
 * created.
 */
uintptr_t sys_thread_create(uintptr_t entry, uintptr_t stack_base,
							uintptr_t stack_size, uintptr_t, uintptr_t,
							uintptr_t);

/**
 * @brief Synchronous call: sends four words to a thread and blocks until it
 * replies; the answer comes back in the registers of the call.
 * @param target Thread id of the server.
 * @param word0 First payload word, truncated to 32 bits.
 * @param word1 Second payload word.
 * @param word2 Third payload word.
 * @param word3 Fourth payload word.
 * @param milliseconds Time limit, the no timeout value to wait forever, or a
 * finite value up to the maximum.
 * @return The call result code: success (reply words are in the result
 * registers), timed out, or failure when the time limit is invalid, the target
 * is dead or the call is refused.
 */
uintptr_t sys_ipc_call(uintptr_t target, uintptr_t word0, uintptr_t word1,
					   uintptr_t word2, uintptr_t word3, uintptr_t milliseconds);

/**
 * @brief Waits for the next synchronous call; the caller id and the four
 * words come back in the registers of the call.
 * @param milliseconds Time limit, the no timeout value to wait forever, or a
 * finite value up to the maximum.
 * @return Success when a request was served or the thread blocks waiting,
 * message pending when an asynchronous message arrived and must be read with
 * try receive, timed out, or failure.
 */
uintptr_t sys_ipc_accept(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t milliseconds);

/**
 * @brief Answers the call being served with four words.
 * @param word0 First reply word.
 * @param word1 Second reply word.
 * @param word2 Third reply word.
 * @param word3 Fourth reply word.
 * @return Success, or failure when the thread is not serving a call.
 */
uintptr_t sys_ipc_reply(uintptr_t, uintptr_t word0, uintptr_t word1, uintptr_t word2,
						uintptr_t word3, uintptr_t);

/**
 * @brief Answers the call being served, if any, and waits for the next one.
 * @param word0 First reply word.
 * @param word1 Second reply word.
 * @param word2 Third reply word.
 * @param word3 Fourth reply word.
 * @param milliseconds Time limit for the wait, as in sys_ipc_accept.
 * @return The same result codes as sys_ipc_accept, or failure when the reply
 * is refused or the time limit is invalid.
 */
uintptr_t sys_ipc_reply_wait(uintptr_t, uintptr_t word0, uintptr_t word1,
							 uintptr_t word2, uintptr_t word3, uintptr_t milliseconds);

/**
 * @brief Detaches the call being served and returns its ticket, so the server can
 * accept other calls and answer this one later with sys_ipc_reply_to.
 * @return The ticket (the thread id of the caller), or failure when no call is
 * being served.
 */
uintptr_t sys_ipc_defer(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						uintptr_t);

/**
 * @brief Answers a detached call, named by its ticket, with four words.
 * @param ticket Value returned by sys_ipc_defer.
 * @param word0 First reply word.
 * @param word1 Second reply word.
 * @param word2 Third reply word.
 * @param word3 Fourth reply word.
 * @return Success, or failure when the ticket does not name a call detached
 * by the caller.
 */
uintptr_t sys_ipc_reply_to(uintptr_t ticket, uintptr_t word0, uintptr_t word1,
						   uintptr_t word2, uintptr_t word3, uintptr_t);

/**
 * @brief Writes the scheduler figures of the calling thread (`abi::sched::Metrics`).
 * @param buffer User address of the structure to fill.
 * @return Success, or failure when the buffer is not writable user memory or the
 * kernel was built without `SCHED_METRICS=1` (the call is then not registered).
 */
uintptr_t sys_sched_metrics(uintptr_t buffer, uintptr_t, uintptr_t, uintptr_t,
							uintptr_t, uintptr_t);

/**
 * @brief Retrieves the thread id of the calling thread.
 * @return The thread id, or all ones when there is no current thread.
 */
uintptr_t sys_gettid(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
					 uintptr_t);

/**
 * @brief Blocks while the word at the address still holds the expected value.
 * @param address User address of an aligned 32 bit word.
 * @param expected Value the word must have for the thread to sleep.
 * @param milliseconds Time limit, the futex no timeout value to wait forever,
 * or a finite value up to the maximum.
 * @return Woken, timed out or value changed, as defined by the process ABI.
 * All ones when the address is null, unaligned or not readable, or the time
 * limit is invalid. The call is restarted internally while it sleeps.
 */
uintptr_t sys_futex_wait(uintptr_t address, uintptr_t expected, uintptr_t milliseconds,
						 uintptr_t, uintptr_t, uintptr_t);

/**
 * @brief Wakes up to count threads of the caller process waiting on the word.
 * @param address User address of the aligned 32 bit word.
 * @param count Maximum number of threads to wake.
 * @return The number of threads woken, or all ones for an invalid address.
 */
uintptr_t sys_futex_wake(uintptr_t address, uintptr_t count, uintptr_t, uintptr_t,
						 uintptr_t, uintptr_t);

} // namespace cock::core::syscall

#endif
