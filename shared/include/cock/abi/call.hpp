#ifndef SHARED_COCK_ABI_CALL_HPP
#define SHARED_COCK_ABI_CALL_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::call {

constexpr size_t WORD_COUNT = 4;
constexpr uintptr_t SUCCESS = 0;
constexpr uintptr_t TIMED_OUT = 1;
constexpr uintptr_t MESSAGE_PENDING = 2;
constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
constexpr uint32_t NO_TIMEOUT = 0xFFFFFFFF;
constexpr uintptr_t MAX_TIMEOUT_MILLISECONDS = 0x7FFFFFFF;

} // namespace cock::abi::call

#endif // !SHARED_COCK_ABI_CALL_HPP
