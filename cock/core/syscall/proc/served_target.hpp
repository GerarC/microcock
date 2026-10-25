/** @file
 * @brief The consent a process gives to being replaced or copied by its server: it
 * waits for the reply of a call that the server is serving.
 */
#ifndef CORE_SYSCALL_PROC_SERVED_TARGET_HPP
#define CORE_SYSCALL_PROC_SERVED_TARGET_HPP

#include <cock/core/task/thread.hpp>

namespace cock::core::syscall {

/**
 * @brief Tells whether a server may act on the process of a thread that it serves.
 *
 * The thread waits for the reply of a call, and the server either accepted that call and has not
 * answered, or detached it with `ipc_defer`. That is the consent of the target, and it is not
 * running, so its address space is not in use and its saved context is complete.
 *
 * @param server The thread that serves, normally the caller of the system call.
 * @param target The thread found by an id the caller gave, untrusted; may be null.
 * @return True when the target exists, is the main thread of a user process other than the
 * server's, waits for a reply, and the server serves it. False otherwise.
 */
bool serves_main_thread(const task::Thread &server, const task::Thread *target);

} // namespace cock::core::syscall

#endif // !CORE_SYSCALL_PROC_SERVED_TARGET_HPP
