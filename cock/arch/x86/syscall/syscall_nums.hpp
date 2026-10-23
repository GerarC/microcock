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
    SYS_EXIT                = 0x01,  /**< Terminate the calling process and record its exit status. */
    SYS_READ                = 0x03,  /**< Read from standard input (file descriptor 0 only; the kernel has no input queue, so it validates the buffer and reports end of input with 0; input comes from the console server). */
    SYS_WRITE               = 0x04,  /**< Write at most 128 bytes per call to the console (file descriptor 1 or 2), forwarded to the display server or, without one, to the kernel console; returns the bytes accepted. After 1000 restarts against a full display server inbox it writes to the kernel console instead. */
    SYS_REQUEST_RESOURCE    = 0x07,  /**< Ask for I/O ports (first port and count); allowed only if the caller holds a covering IO_PORTS capability and the range avoids the PIC and timer ports. */
    SYS_GETPID              = 0x16,  /**< Get the id of the calling process. */
    SYS_MMAP                = 0x5A,  /**< Map fresh anonymous pages into the caller, or (with MAP_PHYS and a PHYSICAL_MEMORY capability) device memory at a fixed address. */
    SYS_MUNMAP              = 0x5B,  /**< Unmap a whole region previously mapped (same start address and page count). */
    SYS_SCHED_YIELD         = 0x9E,  /**< Give up the CPU voluntarily. */
    SYS_IPC_SEND            = 0x160, /**< Send an IPC message to the inbox of a thread (a process id reaches its first thread). */
    SYS_IPC_RECV            = 0x161, /**< Block until an interrupt notification or a message in the inbox of the calling thread arrives. */
    SYS_CAP_GRANT           = 0x170, /**< Grant part of a grantable capability held by the caller to another process; returns the id of the derivation tree node, or -1. */
    SYS_IRQ_BIND            = 0x171, /**< Bind a hardware interrupt line to the calling thread (needs an IRQ capability). */
    SYS_IRQ_ACK             = 0x172, /**< Acknowledge a bound interrupt and re-enable the line. */
    SYS_PROC_LOOKUP         = 0x173, /**< Find a process id by its registered name. */
    SYS_SHM_CREATE          = 0x174, /**< Create a shared memory object. */
    SYS_SHM_SHARE           = 0x175, /**< Share a shared memory object with another process. */
    SYS_SHM_MAP             = 0x176, /**< Map a shared memory object into the caller. */
    SYS_SHM_RELEASE         = 0x177, /**< Drop the caller's reference to a shared memory object. */
    SYS_MODULE_OPEN         = 0x178, /**< Open a boot data module by name as a read only shared object (needs a BOOT_MODULE capability). */
    SYS_MODULE_INDEX        = 0x179, /**< Resolve a boot data module name to its index (grant authority only). */
    SYS_SPAWN               = 0x17A, /**< Start a new process from an ELF image in memory (grant authority only). */
    SYS_PROC_ALIVE          = 0x17B, /**< Tell whether the thread with an id (a process id names its first thread) is alive. */
    SYS_IPC_TRY_RECV        = 0x17C, /**< Non blocking IPC receive: 0 message, 1 nothing pending. */
    SYS_IPC_RECV_TIMEOUT    = 0x17D, /**< IPC receive that gives up after a timeout in milliseconds: 0 message, 1 timeout. */
    SYS_SLEEP               = 0x17E, /**< Sleep for a number of milliseconds (0 only yields). */
    SYS_THREAD_CREATE       = 0x17F, /**< Create a thread in the calling process. */
    SYS_FUTEX_WAIT          = 0x180, /**< Block on a user address while it holds an expected value. */
    SYS_FUTEX_WAKE          = 0x181, /**< Wake threads blocked on a user address. */
    SYS_GETTID              = 0x182, /**< Get the id of the calling thread. */
    SYS_IPC_CALL            = 0x183, /**< Synchronous IPC: send four words to a server thread and wait for the reply, with an optional time limit in milliseconds. */
    SYS_IPC_ACCEPT          = 0x184, /**< Server side: wait for the next synchronous request. */
    SYS_IPC_REPLY           = 0x185, /**< Server side: answer the request being served. */
    SYS_IPC_REPLY_WAIT      = 0x186, /**< Server side: answer and wait for the next request in one call. */
    SYS_IPC_DEFER           = 0x187, /**< Server side: postpone the reply of the current request. */
    SYS_IPC_REPLY_TO        = 0x188, /**< Server side: answer a request that was deferred, named by the ticket that SYS_IPC_DEFER returned. */
    SYS_SCHED_METRICS       = 0x189, /**< Write the response, CPU and turnaround ticks of the calling thread; registered only in a kernel built with SCHED_METRICS=1. */
    SYS_CAP_REVOKE          = 0x18A, /**< Revoke a node of the derivation tree and every capability derived from it; the caller must hold an ancestor of the node. */
    SYS_SERVICE_SET         = 0x18B, /**< Give a process its service label, once; the caller must hold grant authority. */
};

}

#endif // !X86_COCK_SYSCALL_NUMS_HPP
