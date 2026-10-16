/**
 * @file syscalls.h
 * @brief Raw system call numbers and the `int 0x80` entry stub (i386).
 *
 * Calling convention: the number goes in EAX; the arguments in EBX, ECX, EDX,
 * ESI, EDI and EBP in that order; the result comes back in EAX (zero or a
 * positive value on success, a negative or all ones value on failure, as
 * documented by each call). The kernel never trusts the numbers or the
 * pointers it receives and validates them before acting. Capability checks
 * apply to the privileged calls.
 */
#ifndef SYS_SYSCALLS_H
#define SYS_SYSCALLS_H

#include <stdint.h>

#if defined(__i386__)
    /** Terminates the calling process. a1: exit status. Does not return. */
    #define SYS_EXIT             0x01
    /** Reads from a descriptor. a1: fd, a2: destination, a3: length. Returns bytes read. */
    #define SYS_READ             0x03
    /** Writes to the kernel debug console. a1: fd, a2: source, a3: length. Returns bytes written. */
    #define SYS_WRITE            0x04
    /** Requests an I/O port range (capability checked). a1: port, a2: value or count. */
    #define SYS_REQUEST_RESOURCE 0x07
    /** Returns the process id of the caller (with its generation counter). */
    #define SYS_GETPID           0x16
    /** Maps pages. a1: address hint, a2: length, a3: protection, a4: flags, a5: fd, a6: offset. Returns the address or all ones. */
    #define SYS_MMAP             0x5A
    /** Unmaps pages. a1: page aligned address, a2: length. Returns 0 or failure. */
    #define SYS_MUNMAP           0x5B
    /** Gives the processor to the next ready task. */
    #define SYS_SCHED_YIELD      0x9E
    /** Sends an asynchronous message. a1: destination process id, a2: pointer to the message. */
    #define SYS_IPC_SEND         0x160
    /** Blocks until a message arrives. a1: pointer where the message is stored. */
    #define SYS_IPC_RECV         0x161
    /** Grants capabilities (init only). a1: process id, a2 to a4: capability data. */
    #define SYS_CAP_GRANT        0x170
    /** Binds an interrupt line to the caller as messages. a1: IRQ number. */
    #define SYS_IRQ_BIND         0x171
    /** Acknowledges and unmasks a bound interrupt. a1: IRQ number. */
    #define SYS_IRQ_ACK          0x172
    /** Looks up a process id by name. a1: name pointer, a2: name length. */
    #define SYS_PROC_LOOKUP      0x173
    /** Creates a shared memory object. a1: pages. Returns its id. */
    #define SYS_SHM_CREATE       0x174
    /** Shares an object with another process. a1: object, a2: process id, a3: flags. */
    #define SYS_SHM_SHARE        0x175
    /** Maps a shared object. a1: object, a2: flags. Returns the address. */
    #define SYS_SHM_MAP          0x176
    /** Drops the reference of the caller to a shared object. a1: object. */
    #define SYS_SHM_RELEASE      0x177
    /** Opens a boot data module by name. a1: name pointer, a2: name length. */
    #define SYS_MODULE_OPEN      0x178
    /** Resolves a boot module name to its index. a1: name pointer, a2: name length. */
    #define SYS_MODULE_INDEX     0x179
    /** Starts a process from an ELF image. a1: image pointer, a2: image length, a3: name pointer, a4: name length, a5: flags. */
    #define SYS_SPAWN            0x17A
    /** Tells whether a process id is still alive. a1: process id. */
    #define SYS_PROC_ALIVE       0x17B
    /** Receives a message without blocking. a1: pointer where the message is stored. */
    #define SYS_IPC_TRY_RECV     0x17C
    /** Receives a message with a time limit. a1: message pointer, a2: milliseconds. */
    #define SYS_IPC_RECV_TIMEOUT 0x17D
    /** Sleeps. a1: milliseconds. */
    #define SYS_SLEEP            0x17E
    /** Creates a thread in the caller process. a1: entry, a2: stack base, a3: stack size. */
    #define SYS_THREAD_CREATE    0x17F
    /** Waits on a futex word while it holds a value. a1: aligned address, a2: expected value, a3: milliseconds. */
    #define SYS_FUTEX_WAIT       0x180
    /** Wakes futex waiters. a1: aligned address, a2: maximum number of waiters. */
    #define SYS_FUTEX_WAKE       0x181
    /** Returns the thread id of the caller. */
    #define SYS_GETTID           0x182
    /** Synchronous call: sends a request and blocks for the reply. a1: process id, a2 to a5: four payload words, a6: milliseconds. */
    #define SYS_IPC_CALL         0x183
    /** Server side: waits for the next synchronous request. a6: milliseconds. */
    #define SYS_IPC_ACCEPT       0x184
    /** Server side: answers the request being served. a2 to a5: four payload words. */
    #define SYS_IPC_REPLY        0x185
    /** Server side: answers the current request and waits for the next. a2 to a5: payload, a6: milliseconds. */
    #define SYS_IPC_REPLY_WAIT   0x186
    /** Server side: postpones the answer of the current request so another can be accepted. */
    #define SYS_IPC_DEFER        0x187
    /** Server side: answers a deferred request. a1: client process id, a2 to a5: four payload words. */
    #define SYS_IPC_REPLY_TO     0x188

    #ifdef __cplusplus
    extern "C" {
    #endif

    /**
     * @brief Performs a system call with `int 0x80`.
     * @param num System call number (one of `SYS_*`).
     * @param a1 First argument (EBX).
     * @param a2 Second argument (ECX).
     * @param a3 Third argument (EDX).
     * @param a4 Fourth argument (ESI).
     * @param a5 Fifth argument (passed in EDI through a spill slot).
     * @param a6 Sixth argument (passed in EBP through a spill slot).
     * @return The value of EAX after the call. Unused arguments must be zero.
     */
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
