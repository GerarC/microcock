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
    /** Terminates the calling process. a1: exit status, kept for the spawner that asked for `PROCESS_EXITED`. Does not return. */
    #define SYS_EXIT             0x1
    /** Reads from standard input. a1: fd (only 0 is accepted), a2: destination, a3: length. The kernel has no input queue: it validates the buffer as writable user memory and reports end of input with 0 (input comes from the console server); returns all ones for another fd or a bad buffer. */
    #define SYS_READ             0x3
    /** Writes text to the console. a1: fd (1 or 2, both go to the same output), a2: source, a3: length. At most 128 bytes are taken per call and the length is cut to that, so the caller must loop on the returned count. The text goes to the display server, or to the kernel console when there is none. Returns the bytes accepted (can be less than the length), or all ones for another fd or a bad buffer. */
    #define SYS_WRITE            0x4
    /** Returns the process id of the caller (with its generation counter); it is also the id of the first thread of the process. */
    #define SYS_GETPID           0x14
    /** Maps pages. a1: address (ignored for anonymous memory; the required fixed target for `MAP_PHYS`, rounded down to a page), a2: length in bytes, rounded up to pages, a3: protection (`PROT_WRITE` 0x2 gives a read write mapping, `PROT_READ` 0x1 or `PROT_EXEC` 0x4 alone a read only one; zero or any other bit is refused), a4: flags (`MAP_ANONYMOUS` 0x20, or `MAP_PHYS` 0x1000, which also needs a `PHYSICAL_MEMORY` capability and device memory), a5: fd (ignored), a6: physical address for `MAP_PHYS`. Returns the address or all ones. */
    #define SYS_MMAP             0x5A
    /** Unmaps a region returned by a mapping call. a1: page aligned start address, a2: length, which must give the same number of pages as the mapping (a part of a region is refused). Returns 0 or all ones. */
    #define SYS_MUNMAP           0x5B
    /** Gives the processor to the next ready task. */
    #define SYS_SCHED_YIELD      0x9E
    /** Returns the thread id of the caller. */
    #define SYS_GETTID           0xE0

    /** Creates a process with an empty address space and no thread; needs a `SPAWN` capability. a1: name pointer, a2: name length, a3: flags (`SPAWN_NOTIFY_EXIT` asks for a `PROCESS_EXITED` message), a4: pointer to `SpawnRights` (the service label and the IPC labels of the new process) or 0, a5: user space limit of the process (page aligned, above the user stack). Returns a handle for the pending process, or all ones when the caller already holds `MAX_PENDING_PER_PROCESS` of them or the system `MAX_PENDING_PROCESSES`, ids or memory ran out, or an argument is invalid. */
    #define SYS_PROC_CREATE      0x200
    /** Maps fresh zeroed writable pages into a pending process the caller created. a1: handle, a2: page aligned address (not the first page, below the user stack), a3: number of pages. At most `MAX_PENDING_IMAGE_BYTES` per process; the range must be unmapped. Returns 0 or all ones. */
    #define SYS_PROC_MAP         0x201
    /** Copies bytes into mapped, still writable pages of a pending process the caller created. a1: handle, a2: destination address in that process, a3: source address in the caller, a4: length. Returns 0 or all ones. */
    #define SYS_PROC_WRITE       0x202
    /** Makes mapped pages of a pending process read only and executable. a1: handle, a2: page aligned address, a3: number of pages. Returns 0 or all ones. */
    #define SYS_PROC_PROTECT     0x203
    /** Creates the first thread of a pending process at an entry point and lets it run; applies the rights and the exit notice asked at creation. a1: handle, a2: entry point (inside a mapped page). Returns the process id and invalidates the handle, or all ones. */
    #define SYS_PROC_START       0x204
    /** Destroys a pending process the caller created and frees its memory. a1: handle. Returns 0 or all ones. */
    #define SYS_PROC_DISCARD     0x205
    /** Tells whether the thread with an id (a process id names its first thread) exists and is not dead. a1: id. Returns 0 when alive, all ones otherwise. */
    #define SYS_PROC_ALIVE       0x206
    /** Creates a thread in the caller process on a stack that the caller owns. a1: entry, a2: stack base, a3: stack size (at least 8 KiB). Returns the thread id or all ones. */
    #define SYS_THREAD_CREATE    0x207
    /** Gives the process a thread belongs to. a1: thread id. Returns the process id, or all ones when the thread is unknown, dead or the kernel's. */
    #define SYS_THREAD_PROCESS   0x208
    /** Tells in which states the live threads of a process are. a1: process id. Returns a mask (1 ready, 2 running, 4 blocked, 8 waiting for a message, 16 waiting in accept, 32 waiting for a reply), or all ones when the id is not a process id, is the kernel's or has no live thread. */
    #define SYS_PROC_STATE       0x209

    /** Sleeps; messages that arrive meanwhile do not wake the thread. a1: milliseconds (at most 0x7FFFFFFF; 0 only yields). Returns 0, or all ones when the time is too large. */
    #define SYS_SLEEP            0x210
    /** Waits on a futex word while it holds a value. a1: aligned address, a2: expected value, a3: milliseconds (0xFFFFFFFF waits forever). Returns 0 when woken, 1 when the word differed, 2 on timeout, all ones for a bad address or time. */
    #define SYS_FUTEX_WAIT       0x211
    /** Wakes futex waiters of the caller process. a1: aligned address, a2: maximum number of waiters. Returns the number woken or all ones for a bad address. */
    #define SYS_FUTEX_WAKE       0x212
    /** Writes the scheduler figures of the calling thread. a1: address of a `cock::abi::sched::Metrics`. Fails unless the kernel was built with `SCHED_METRICS=1`. */
    #define SYS_SCHED_METRICS    0x213

    /** Sends an asynchronous message to the inbox of one thread (a process id reaches its first thread). a1: destination id, a2: pointer to the 'Message' (the kernel stamps the sender thread id). Returns 0, or all ones when the target is missing or dead, its inbox is full, the pointer is bad or the type is reserved to the kernel or to grant authority. */
    #define SYS_IPC_SEND         0x220
    /** Blocks until an interrupt notification or a message in the inbox of the calling thread arrives. a1: pointer where the message is stored. Returns 0, or all ones for a bad pointer. */
    #define SYS_IPC_RECV         0x221
    /** Receives a message without blocking. a1: pointer where the message is stored. Returns 0 when a message was stored, 1 when nothing is pending, all ones for a bad pointer. */
    #define SYS_IPC_TRY_RECV     0x222
    /** Receives a message with a time limit. a1: message pointer, a2: milliseconds (at most 0x7FFFFFFF; 0 never waits). Returns 0 when a message was stored, 1 on timeout, all ones for a bad pointer or time. */
    #define SYS_IPC_RECV_TIMEOUT 0x223
    /** Sends an asynchronous message through a handle on an endpoint; the kernel stamps the badge of the handle in the message. a1: handle (an ENDPOINT capability node id of the caller), a2: pointer to the 'Message', a3: flags (`SEND_EXPECT_REPLY` opens a reply token that the receiver finds in `Message::reply`). Returns 0, or all ones when the handle is not held, the table of tokens is full, the endpoint is unbound or its process is gone, the inbox is full, the pointer is bad or the type is reserved. */
    #define SYS_EP_SEND          0x224
    /** Answers a message that carried a reply token. a1: the token (`Message::reply`), a2: pointer to the 'Message'. The answer goes to the thread that asked, with the id of the caller as sender; the token is spent. Returns 0, or all ones when the caller does not hold the token, the requester is gone or its inbox is full (the token stays open then), the pointer is bad or the type is reserved or a capability message. */
    #define SYS_EP_REPLY         0x225
    /** Passes a message that carried a reply token to another endpoint. a1: the token, a2: handle of the caller on the endpoint, a3: pointer to the 'Message'. The token goes with the message and the new receiver holds it. Returns 0, or all ones when the caller does not hold the token, the handle leads nowhere, its inbox is full (the caller keeps the token), the pointer is bad or the type is reserved or a capability message. */
    #define SYS_EP_FORWARD       0x226

    /** Synchronous call: sends a request and blocks for the reply. a1: id of the server thread, a2 to a5: four payload words, a6: milliseconds (0xFFFFFFFF waits forever). The reply words come back in the result registers. */
    #define SYS_IPC_CALL         0x230
    /** Server side: waits for the next synchronous request; the caller id and the four words come back in the result registers. a6: milliseconds (0xFFFFFFFF waits forever). It can also report that an asynchronous message is pending. */
    #define SYS_IPC_ACCEPT       0x231
    /** Server side: answers the request being served. a2 to a5: four payload words. Fails when no request is being served. */
    #define SYS_IPC_REPLY        0x232
    /** Server side: answers the current request, if any, and waits for the next. a2 to a5: payload, a6: milliseconds. */
    #define SYS_IPC_REPLY_WAIT   0x233
    /** Server side: postpones the answer of the current request so another can be accepted; returns a ticket (the id of the caller thread). */
    #define SYS_IPC_DEFER        0x234
    /** Server side: answers a deferred request. a1: ticket returned by `SYS_IPC_DEFER`, a2 to a5: four payload words. */
    #define SYS_IPC_REPLY_TO     0x235
    /** Synchronous call through a handle on an endpoint. a1: handle, a2 to a5: four payload words, a6: milliseconds. The server reads the badge of the handle as the peer of its accept. */
    #define SYS_EP_CALL          0x236

    /** Grants a capability to another process; the caller must hold a grantable capability that covers the range. a1: target process id, a2: capability type, a3: first port, line or module index, a4: count, a5: flags (GRANT_DELEGATE and GRANT_MINT), a6: badge of an ENDPOINT capability. Returns the node id of the copy or all ones. */
    #define SYS_CAP_GRANT        0x240
    /** Revokes a node of the derivation tree and everything derived from it; the caller must hold an ancestor of the node. a1: node id returned by `SYS_CAP_GRANT`. Returns 0 or all ones. */
    #define SYS_CAP_REVOKE       0x241
    /** Chooses which capability types a CAPABILITY_TRANSFER message may give the caller. a1: mask with one bit per capability type (1 << type), no bit outside the valid types. Replaces the previous mask; none is accepted until the first call. Returns 0 or all ones. */
    #define SYS_CAP_ACCEPT       0x242
    /** Gives a process its service label, once; needs grant authority. a1: process id, a2: label. Returns 0 or all ones. */
    #define SYS_SERVICE_SET      0x243
    /** Creates an endpoint; needs grant authority. a1: pointer to a 32 bit value that receives the handle of the owner capability. Returns the endpoint id (from 1) or all ones. */
    #define SYS_EP_CREATE        0x244
    /** Binds an endpoint to the process that receives from it; needs the bind right. a1: endpoint id, a2: process id. Returns 0 or all ones. */
    #define SYS_EP_BIND          0x245

    /** Requests an I/O port range (needs an `IO_PORTS` capability that covers it; the PIC and timer ports are refused). a1: first port, a2: number of consecutive ports. Returns 0 or all ones. */
    #define SYS_REQUEST_RESOURCE 0x250
    /** Binds an interrupt line to the calling thread; each interrupt arrives as an `IRQ_NOTIFICATION` message (needs an `IRQ` capability that covers the line). a1: IRQ number. Returns 0 or all ones. */
    #define SYS_IRQ_BIND         0x251
    /** Acknowledges and unmasks an interrupt line that the calling thread bound. a1: IRQ number. Returns 0 or all ones. */
    #define SYS_IRQ_ACK          0x252

    /** Creates a shared memory object held by the caller. a1: pages. Returns its id or all ones. */
    #define SYS_SHM_CREATE       0x260
    /** Makes another live process a holder of an object; only a holder can share and nobody gives more access than it has. a1: object, a2: process id, a3: non zero for write access. Returns 0 or all ones. */
    #define SYS_SHM_SHARE        0x261
    /** Maps a shared object into the caller; the caller must be a holder. a1: object, a2: non zero to map it writable. Returns the address or all ones. */
    #define SYS_SHM_MAP          0x262
    /** Drops the hold of the caller on a shared object. a1: object. Returns 0 or all ones. */
    #define SYS_SHM_RELEASE      0x263

    /** Finds the live process the kernel started from a boot module, by module name; only a process with grant authority may call it. a1: name pointer, a2: name length (not counting the terminator). Returns the id or all ones. */
    #define SYS_MODULE_PROCESS   0x270
    /** Opens a boot data module by name as a read only shared object (needs a `BOOT_MODULE` capability that covers it). a1: name pointer, a2: name length. Returns the object id or all ones. */
    #define SYS_MODULE_OPEN      0x271
    /** Resolves a boot module name to its index; only a process with grant authority may call it. a1: name pointer, a2: name length. Returns the index or all ones. */
    #define SYS_MODULE_INDEX     0x272

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
