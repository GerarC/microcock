#ifndef SYS_SYSCALLS_H
#define SYS_SYSCALLS_H

#include <stdint.h>

#if defined(__i386__)
    #define SYS_EXIT             0x01
    #define SYS_READ             0x03
    #define SYS_WRITE            0x04
    #define SYS_REQUEST_RESOURCE 0x07
    #define SYS_GETPID           0x16
    #define SYS_MMAP             0x5A
    #define SYS_MUNMAP           0x5B
    #define SYS_SCHED_YIELD      0x9E
    #define SYS_IPC_SEND         0x160
    #define SYS_IPC_RECV         0x161

    #ifdef __cplusplus
    extern "C" {
    #endif

    static inline uintptr_t syscall(uint32_t num, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t a6) {
        uintptr_t ret;
        __asm__ volatile(
            "push %%ebp; mov %7, %%ebp; int $0x80; pop %%ebp"
            : "=a"(ret)
            : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5), "m"(a6)
            : "memory"
        );
        return ret;
    }

    #ifdef __cplusplus
    }
    #endif
#else
    #error "Not supported architecture"
#endif

#endif // SYS_SYSCALLS_H
