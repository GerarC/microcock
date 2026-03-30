#ifndef SHARED_COCK_SYSCALL_NUMS_HPP
#define SHARED_COCK_SYSCALL_NUMS_HPP

#include <stdint.h>

namespace cock::syscall {

enum SyscallNum : uint32_t {
    SYS_RESTART_SYSCALL = 0,
    SYS_EXIT            = 1,
    SYS_FORK            = 2,
    SYS_READ            = 3,
    SYS_WRITE           = 4,
    SYS_OPEN            = 5,
    SYS_CLOSE           = 6,
    SYS_GETPID          = 20,
    SYS_SCHED_YIELD     = 158,
};
}

#endif // !SHARED_COCK_SYSCALL_NUMS_HPP
