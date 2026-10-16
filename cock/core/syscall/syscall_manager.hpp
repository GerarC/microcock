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
     * @return The handler, or nullptr when the number is out of range or not
     * implemented. Callers must check for null.
     */
    static SystemCall getSyscall(size_t code);
};

} // namespace cock::core::syscall

#endif // !CORE_SYSCALL_MANAGER_API_HPP
