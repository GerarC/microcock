#ifndef SHARED_COCK_ABI_PROCESS_HPP
#define SHARED_COCK_ABI_PROCESS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::process {

constexpr size_t MAX_THREADS_PER_PROCESS = 8;
constexpr size_t MIN_THREAD_STACK_BYTES = 0x2000;
constexpr uint32_t NO_PARENT = 0xFFFFFFFF;
constexpr uint32_t FAULT_STATUS = 0xFFFFFFFF;
constexpr uintptr_t SPAWN_NOTIFY_EXIT = 1;

constexpr uint32_t FUTEX_NO_TIMEOUT = 0xFFFFFFFF;
constexpr uintptr_t FUTEX_WOKEN = 0;
constexpr uintptr_t FUTEX_VALUE_CHANGED = 1;
constexpr uintptr_t FUTEX_TIMED_OUT = 2;
constexpr uintptr_t FUTEX_WORD_ALIGNMENT = sizeof(uint32_t);

} // namespace cock::abi::process

#endif // !SHARED_COCK_ABI_PROCESS_HPP
