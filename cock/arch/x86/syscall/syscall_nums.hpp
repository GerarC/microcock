#ifndef X86_COCK_SYSCALL_NUMS_HPP
#define X86_COCK_SYSCALL_NUMS_HPP

#include <stdint.h>

namespace cock::arch::x86 {

enum SyscallNum : uint32_t {
    SYS_EXIT                = 0x01,
    SYS_READ                = 0x03,
    SYS_WRITE               = 0x04,
    SYS_REQUEST_RESOURCE    = 0x07,
    SYS_GETPID              = 0x16,
    SYS_MMAP                = 0x5A,
    SYS_MUNMAP              = 0x5B,
    SYS_SCHED_YIELD         = 0x9E,
    SYS_IPC_SEND            = 0x160,
    SYS_IPC_RECV            = 0x161,
};

}

#endif // !X86_COCK_SYSCALL_NUMS_HPP
