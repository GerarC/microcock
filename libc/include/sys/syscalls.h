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
    #define SYS_CAP_GRANT        0x170
    #define SYS_IRQ_BIND         0x171
    #define SYS_IRQ_ACK          0x172
    #define SYS_PROC_LOOKUP      0x173
    #define SYS_SHM_CREATE       0x174
    #define SYS_SHM_SHARE        0x175
    #define SYS_SHM_MAP          0x176
    #define SYS_SHM_RELEASE      0x177

    #ifdef __cplusplus
    extern "C" {
    #endif

    static inline uintptr_t syscall(uint32_t num, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t a6) {
        uintptr_t ret = num;
        uintptr_t last_args[2] = {a5, a6};
        uintptr_t *last_args_pointer = last_args;
        __asm__ volatile(
            "push %%ebp; mov 4(%%edi), %%ebp; mov (%%edi), %%edi; int $0x80; pop %%ebp"
            : "+a"(ret), "+D"(last_args_pointer)
            : "b"(a1), "c"(a2), "d"(a3), "S"(a4), "m"(last_args)
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
