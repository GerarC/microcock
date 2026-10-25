/**
 * @file process.hpp
 * @brief Process, thread and futex constants shared by the kernel and user space.
 */
#ifndef SHARED_COCK_ABI_PROCESS_HPP
#define SHARED_COCK_ABI_PROCESS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::process {

/** Most threads a process can have, including the first. */
constexpr size_t MAX_THREADS_PER_PROCESS = 8;
/** Smallest stack size `SYS_THREAD_CREATE` accepts. */
constexpr size_t MIN_THREAD_STACK_BYTES = 0x2000;
/** Parent id of a process that has no parent (started by the kernel). */
constexpr uint32_t NO_PARENT = 0xFFFFFFFF;
/** Exit status reported in `PROCESS_EXITED` when a CPU exception ended the process. */
constexpr uint32_t FAULT_STATUS = 0xFFFFFFFF;
/** Flag of `SYS_PROC_CREATE`: send the creator a `PROCESS_EXITED` message when the child ends. */
constexpr uintptr_t SPAWN_NOTIFY_EXIT = 1;
/** Most children with `SPAWN_NOTIFY_EXIT` a process may have at once, running or exited and not yet read; `SYS_PROC_START` fails past it, so no exit notice is ever dropped. */
constexpr size_t MAX_NOTIFIED_CHILDREN = 64;
/** Handle returned by wrappers when `SYS_PROC_CREATE` failed. */
constexpr uint32_t NO_PENDING_PROCESS = 0xFFFFFFFF;
/** Most processes that exist between `SYS_PROC_CREATE` and `SYS_PROC_START` at once, in the whole system. */
constexpr size_t MAX_PENDING_PROCESSES = 8;
/** Most pending processes one process may hold at once. */
constexpr size_t MAX_PENDING_PER_PROCESS = 2;
/** Most bytes of memory that `SYS_PROC_MAP` maps into one pending process. */
constexpr size_t MAX_PENDING_IMAGE_BYTES = 0x400000;
/** Size in bytes of the user stack the kernel maps just below the user space limit when `SYS_PROC_START` creates the first thread; `SYS_PROC_MAP` never maps into it. */
constexpr size_t PENDING_STACK_BYTES = 0x10000;
/** Alignment in bytes of every address and size given to the `SYS_PROC_*` calls that map memory (the page size). */
constexpr size_t PENDING_PAGE_BYTES = 0x1000;

/** Timeout of `SYS_FUTEX_WAIT` that means wait forever. */
constexpr uint32_t FUTEX_NO_TIMEOUT = 0xFFFFFFFF;
/** Result of `SYS_FUTEX_WAIT`: woken by `SYS_FUTEX_WAKE`. */
constexpr uintptr_t FUTEX_WOKEN = 0;
/** Result of `SYS_FUTEX_WAIT`: the word no longer held the expected value, so the thread did not sleep. */
constexpr uintptr_t FUTEX_VALUE_CHANGED = 1;
/** Result of `SYS_FUTEX_WAIT`: the time limit ran out. */
constexpr uintptr_t FUTEX_TIMED_OUT = 2;
/** `count` of `SYS_FUTEX_WAKE` that wakes a single waiter. */
constexpr uint32_t FUTEX_WAKE_ONE = 1;
/** `count` of `SYS_FUTEX_WAKE` that wakes every waiter (more than a process has threads). */
constexpr uint32_t FUTEX_WAKE_ALL = 0xFFFFFFFF;
/** Required alignment in bytes of a futex word (4); unaligned addresses are refused. */
constexpr uintptr_t FUTEX_WORD_ALIGNMENT = sizeof(uint32_t);

} // namespace cock::abi::process

#endif // !SHARED_COCK_ABI_PROCESS_HPP
