#ifndef CORE_SYSCALL_MANAGER_HPP
#define CORE_SYSCALL_MANAGER_HPP

#include <stdint.h>

namespace cock::core::syscall {

class SyscallManager {
public:
    static uintptr_t handle(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3);
};

} // namespace cock::core::task

#endif
