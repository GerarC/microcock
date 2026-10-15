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
    SYS_CAP_GRANT           = 0x170,
    SYS_IRQ_BIND            = 0x171,
    SYS_IRQ_ACK             = 0x172,
    SYS_PROC_LOOKUP         = 0x173,
    SYS_SHM_CREATE          = 0x174,
    SYS_SHM_SHARE           = 0x175,
    SYS_SHM_MAP             = 0x176,
    SYS_SHM_RELEASE         = 0x177,
    SYS_MODULE_OPEN         = 0x178,
    SYS_MODULE_INDEX        = 0x179,
    SYS_SPAWN               = 0x17A,
    SYS_PROC_ALIVE          = 0x17B,
    SYS_IPC_TRY_RECV        = 0x17C,
    SYS_IPC_RECV_TIMEOUT    = 0x17D,
    SYS_SLEEP               = 0x17E,
    SYS_THREAD_CREATE       = 0x17F,
    SYS_FUTEX_WAIT          = 0x180,
    SYS_FUTEX_WAKE          = 0x181,
    SYS_GETTID              = 0x182,
};

}

#endif // !X86_COCK_SYSCALL_NUMS_HPP
