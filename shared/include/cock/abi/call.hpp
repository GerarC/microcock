/**
 * @file call.hpp
 * @brief Result codes and limits of the synchronous IPC calls (`SYS_IPC_CALL`, `SYS_IPC_ACCEPT`, `SYS_IPC_REPLY*`).
 *
 * A synchronous call carries `WORD_COUNT` 32 bit payload words in each direction. The kernel returns one of
 * the result codes below in EAX; timeouts are in milliseconds.
 */
#ifndef SHARED_COCK_ABI_CALL_HPP
#define SHARED_COCK_ABI_CALL_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::call {

/** Payload words of a request or a reply (a1 to a4 words of the call, 16 bytes). */
constexpr size_t WORD_COUNT = 4;
/** Result: the call completed; the payload words are valid. */
constexpr uintptr_t SUCCESS = 0;
/** Result: the time limit ran out before the peer answered or a request arrived. */
constexpr uintptr_t TIMED_OUT = 1;
/** Result of an accept or reply-and-wait: an asynchronous message arrived instead of a request; the server must read it with `ipc_try_recv`. */
constexpr uintptr_t MESSAGE_PENDING = 2;
/** Result: the arguments were invalid, the peer is dead or the call is not allowed. */
constexpr uintptr_t FAILURE = static_cast<uintptr_t>(-1);
/** Timeout value that means wait forever. */
constexpr uint32_t NO_TIMEOUT = 0xFFFFFFFF;
/** Largest finite timeout in milliseconds the kernel accepts; larger values (other than `NO_TIMEOUT`) are refused. */
constexpr uintptr_t MAX_TIMEOUT_MILLISECONDS = 0x7FFFFFFF;

} // namespace cock::abi::call

#endif // !SHARED_COCK_ABI_CALL_HPP
