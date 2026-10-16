#ifndef X86_COCK_SYSCALL_NUMS_HPP
#define X86_COCK_SYSCALL_NUMS_HPP

/**
 * @file
 * @brief System call numbers of the i386 ABI.
 *
 * ABI: Ring 3 loads the number in EAX and up to six arguments in EBX, ECX,
 * EDX, ESI, EDI and EBP, then executes `int 0x80`; the result comes back in
 * EAX. Every argument is untrusted and validated by the kernel. The numbers for
 * exit, read, write, mmap, munmap and sched_yield reuse the Linux i386 values;
 * the rest are microkernel specific and start at 0x160.
 */

#include <stdint.h>

namespace cock::arch::x86 {

/**
 * @brief Identifier of each system call, stored in EAX before `int 0x80`.
 */
enum SyscallNum : uint32_t {
    SYS_EXIT                = 0x01,  /**< Terminate the calling process. */
    SYS_READ                = 0x03,  /**< Read from standard input (file descriptor 0 only; not implemented yet, it validates the buffer and returns 0). */
    SYS_WRITE               = 0x04,  /**< Write bytes to the console (forwarded to the display server). */
    SYS_REQUEST_RESOURCE    = 0x07,  /**< Ask for I/O ports (first port and count); allowed only if the caller holds a covering IO_PORTS capability. */
    SYS_GETPID              = 0x16,  /**< Get the id of the calling process. */
    SYS_MMAP                = 0x5A,  /**< Map fresh anonymous pages into the caller. */
    SYS_MUNMAP              = 0x5B,  /**< Unmap pages previously mapped. */
    SYS_SCHED_YIELD         = 0x9E,  /**< Give up the CPU voluntarily. */
    SYS_IPC_SEND            = 0x160, /**< Send an IPC message to a process. */
    SYS_IPC_RECV            = 0x161, /**< Block until an IPC message arrives. */
    SYS_CAP_GRANT           = 0x170, /**< Grant a capability to another process. */
    SYS_IRQ_BIND            = 0x171, /**< Bind a hardware interrupt line to the caller. */
    SYS_IRQ_ACK             = 0x172, /**< Acknowledge a bound interrupt and re-enable the line. */
    SYS_PROC_LOOKUP         = 0x173, /**< Find a process id by its registered name. */
    SYS_SHM_CREATE          = 0x174, /**< Create a shared memory object. */
    SYS_SHM_SHARE           = 0x175, /**< Share a shared memory object with another process. */
    SYS_SHM_MAP             = 0x176, /**< Map a shared memory object into the caller. */
    SYS_SHM_RELEASE         = 0x177, /**< Drop the caller's reference to a shared memory object. */
    SYS_MODULE_OPEN         = 0x178, /**< Open a boot data module by name. */
    SYS_MODULE_INDEX        = 0x179, /**< Look up a boot data module by index. */
    SYS_SPAWN               = 0x17A, /**< Start a new process from an executable image. */
    SYS_PROC_ALIVE          = 0x17B, /**< Tell whether a process is still alive. */
    SYS_IPC_TRY_RECV        = 0x17C, /**< Non blocking IPC receive. */
    SYS_IPC_RECV_TIMEOUT    = 0x17D, /**< IPC receive that gives up after a timeout. */
    SYS_SLEEP               = 0x17E, /**< Sleep for a number of ticks. */
    SYS_THREAD_CREATE       = 0x17F, /**< Create a thread in the calling process. */
    SYS_FUTEX_WAIT          = 0x180, /**< Block on a user address while it holds an expected value. */
    SYS_FUTEX_WAKE          = 0x181, /**< Wake threads blocked on a user address. */
    SYS_GETTID              = 0x182, /**< Get the id of the calling thread. */
    SYS_IPC_CALL            = 0x183, /**< Synchronous IPC: send a request and wait for the reply. */
    SYS_IPC_ACCEPT          = 0x184, /**< Server side: wait for the next synchronous request. */
    SYS_IPC_REPLY           = 0x185, /**< Server side: answer the request being served. */
    SYS_IPC_REPLY_WAIT      = 0x186, /**< Server side: answer and wait for the next request in one call. */
    SYS_IPC_DEFER           = 0x187, /**< Server side: postpone the reply of the current request. */
    SYS_IPC_REPLY_TO        = 0x188, /**< Server side: answer a request that was deferred. */
};

}

#endif // !X86_COCK_SYSCALL_NUMS_HPP
