#ifndef CORE_SYSCALL_MANAGER_API_HPP
#define CORE_SYSCALL_MANAGER_API_HPP

#include <cock/core/syscall/syscalls.hpp>
#include <stddef.h>

namespace cock::core::syscall {

class SyscallManager {
public:
	static void init();
    static SystemCall getSyscall(size_t code);
};

} // namespace cock::core::syscall

#endif // !CORE_SYSCALL_MANAGER_API_HPP
