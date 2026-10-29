/** @file
 * @brief Syscall table: maps system call numbers to their handlers.
 */
#ifndef CORE_SYSCALL_MANAGER_API_HPP
#define CORE_SYSCALL_MANAGER_API_HPP

#include <cock/core/syscall/syscalls.hpp>
#include <stddef.h>

namespace cock::core::syscall {

/**
 * @brief Static dispatch table of system calls.
 */
class SyscallManager {
public:
	/**
	 * @brief Fills the table with every handler.
	 * @note Called once during kernel start, before user code runs.
	 */
	static void init();
    /**
     * @brief Looks a handler up.
     * @param code System call number, untrusted.
     * @return The handler. Never null: a number that is out of range or has no
     * implementation gets `sys_not_implemented`, which fails, and is logged at
     * trace level.
     */
    static SystemCall getSyscall(size_t code);
};

} // namespace cock::core::syscall

#endif // !CORE_SYSCALL_MANAGER_API_HPP
