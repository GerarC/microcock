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
/** Flag of `SYS_SPAWN`: send the spawner a `PROCESS_EXITED` message when the child ends. */
constexpr uintptr_t SPAWN_NOTIFY_EXIT = 1;
/** Most children with `SPAWN_NOTIFY_EXIT` a process may have at once, running or exited and not yet read; `SYS_SPAWN` fails past it, so no exit notice is ever dropped. */
constexpr size_t MAX_NOTIFIED_CHILDREN = 16;

/** Timeout of `SYS_FUTEX_WAIT` that means wait forever. */
constexpr uint32_t FUTEX_NO_TIMEOUT = 0xFFFFFFFF;
/** Result of `SYS_FUTEX_WAIT`: woken by `SYS_FUTEX_WAKE`. */
constexpr uintptr_t FUTEX_WOKEN = 0;
/** Result of `SYS_FUTEX_WAIT`: the word no longer held the expected value, so the thread did not sleep. */
constexpr uintptr_t FUTEX_VALUE_CHANGED = 1;
/** Result of `SYS_FUTEX_WAIT`: the time limit ran out. */
constexpr uintptr_t FUTEX_TIMED_OUT = 2;
/** Required alignment in bytes of a futex word (4); unaligned addresses are refused. */
constexpr uintptr_t FUTEX_WORD_ALIGNMENT = sizeof(uint32_t);

} // namespace cock::abi::process

#endif // !SHARED_COCK_ABI_PROCESS_HPP
