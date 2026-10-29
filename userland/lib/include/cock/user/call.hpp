/**
 * @file call.hpp
 * @brief Wrappers for the synchronous, register based IPC calls (call, accept, reply).
 *
 * A request and its answer are four 32 bit words that travel in registers. There is no
 * message type and no user pointer. A client blocks in `ipc_call` until the server
 * replies; a server loops on `ipc_accept` and `ipc_reply_wait`. The target of a call is
 * a thread id (see `gettid`).
 */
#ifndef COCK_USER_CALL_HPP
#define COCK_USER_CALL_HPP

#include <cock/abi/call.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>

namespace cock::user {

/** Number of 32 bit words in a request or an answer. */
constexpr size_t CALL_WORDS = abi::call::WORD_COUNT;
/** Time value that means wait without limit. */
constexpr uint32_t CALL_NO_TIMEOUT = abi::call::NO_TIMEOUT;

/** Status the kernel returns for a synchronous call. */
enum class CallStatus : uint32_t {
	/** The registers hold a request or an answer. */
	OK = static_cast<uint32_t>(abi::call::SUCCESS),
	/** The time ran out before a call or answer arrived. */
	TIMED_OUT = static_cast<uint32_t>(abi::call::TIMED_OUT),
	/** The wait ended because an asynchronous message or interrupt notification is pending; fetch it with `ipc_try_recv`. */
	MESSAGE_PENDING = static_cast<uint32_t>(abi::call::MESSAGE_PENDING),
	/** The call failed: target missing, dead or the caller itself, server died, or bad time. */
	FAILED = static_cast<uint32_t>(abi::call::FAILURE),
};

/** Outcome of a synchronous call or accept. */
struct CallResult {
	/** true only when `status` is `OK`; the other fields are valid only then. */
	bool ok;
	/** Raw status of the call. */
	CallStatus status;
	/** Identity of the peer. On accept: the badge of the handle when the call came through an endpoint, or the thread id of the caller when it came by thread id. On call: the thread id of the replier. */
	uint32_t peer;
	/** The four words of the request (accept) or of the answer (call). */
	uint32_t words[CALL_WORDS];
};

/**
 * @brief Issues a synchronous IPC system call with the words in registers.
 *
 * The fourth word and the time travel in a small array because `EBP` is needed for the
 * time, so it is safe with frame pointers. Used by the wrappers below.
 * @param number System call number (`SYS_IPC_CALL`, `SYS_IPC_ACCEPT`, ...).
 * @param peer Target thread id or ticket, depending on the call; zero when unused.
 * @param words Four words to send, or null when the call sends none.
 * @param milliseconds Longest wait; `CALL_NO_TIMEOUT` waits without limit, `0` never waits.
 * @return The decoded result. On any status other than `OK` only `status` and `ok` are set.
 */
inline CallResult register_call(uint32_t number, uint32_t peer, const uint32_t *words,
								uint32_t milliseconds) {
	uint32_t tail[2] = {words ? words[3] : 0, milliseconds};
	uintptr_t result = number;
	uintptr_t value = peer;
	uintptr_t first = words ? words[0] : 0;
	uintptr_t second = words ? words[1] : 0;
	uintptr_t third = words ? words[2] : 0;
	uintptr_t fourth = reinterpret_cast<uintptr_t>(tail);
	__asm__ volatile("push %%ebp; mov 4(%%edi), %%ebp; mov (%%edi), %%edi; int $0x80; pop %%ebp"
					 : "+a"(result), "+b"(value), "+c"(first), "+d"(second),
					   "+S"(third), "+D"(fourth)
					 : "m"(tail)
					 : "memory");

	CallResult answer = {};
	answer.status = static_cast<CallStatus>(static_cast<uint32_t>(result));
	answer.ok = answer.status == CallStatus::OK;
	if (!answer.ok) return answer;
	answer.peer = static_cast<uint32_t>(value);
	answer.words[0] = static_cast<uint32_t>(first);
	answer.words[1] = static_cast<uint32_t>(second);
	answer.words[2] = static_cast<uint32_t>(third);
	answer.words[3] = static_cast<uint32_t>(fourth);
	return answer;
}

/**
 * @brief Calls a server thread of the calling process and waits for its answer (`SYS_IPC_CALL`). Blocks.
 *
 * The request is queued on the target (first come, first served). When the target waits
 * in accept, the kernel hands the CPU to it directly. A server of another process is called
 * with `ep_call`.
 * @param target Thread id of the server, which must belong to the calling process.
 * @param words The four request words.
 * @param milliseconds Longest wait; `CALL_NO_TIMEOUT` by default. A late answer fails on the
 *        server side instead of reaching the caller.
 * @return `ok` with the answer words, `TIMED_OUT` when the time ran out, or `FAILED` when
 *         the target does not exist, is dead or is the caller, or died before answering.
 *         A time of `0` is not sent and times out at once.
 */
inline CallResult ipc_call(uint32_t target, const uint32_t (&words)[CALL_WORDS],
						   uint32_t milliseconds = CALL_NO_TIMEOUT) {
	return register_call(SYS_IPC_CALL, target, words, milliseconds);
}

/**
 * @brief Calls the process bound to an endpoint and waits for its answer (`SYS_EP_CALL`). Blocks.
 *
 * Like `ipc_call`, but the destination is a handle the caller holds, not a thread id, and holding it
 * is the permission. The server finds the badge of the handle in the `peer` of its accept.
 * @param handle A handle on an endpoint.
 * @param words The four request words.
 * @param milliseconds Longest wait; `CALL_NO_TIMEOUT` by default.
 * @return As `ipc_call`; `FAILED` also when the handle is not held or the endpoint has no live owner.
 */
inline CallResult ep_call(uint32_t handle, const uint32_t (&words)[CALL_WORDS],
						  uint32_t milliseconds = CALL_NO_TIMEOUT) {
	return register_call(SYS_EP_CALL, handle, words, milliseconds);
}

/**
 * @brief Waits for a call from a client (`SYS_IPC_ACCEPT`). Blocks.
 *
 * The server must answer the call it serves with `ipc_reply`, `ipc_reply_wait`,
 * or detach it with `ipc_defer`.
 * @param milliseconds Longest wait; `CALL_NO_TIMEOUT` by default.
 * @return `ok` with the caller thread id in `peer` and the request words; `TIMED_OUT`;
 *         `MESSAGE_PENDING` when an asynchronous message or interrupt notification is
 *         waiting (calls come first); or `FAILED`.
 */
inline CallResult ipc_accept(uint32_t milliseconds = CALL_NO_TIMEOUT) {
	return register_call(SYS_IPC_ACCEPT, 0, nullptr, milliseconds);
}

/**
 * @brief Answers the call being served (`SYS_IPC_REPLY`). Does not block.
 * @param words The four answer words.
 * @return true when the answer was delivered; false when no call is being served (also
 *         when the caller already timed out or died).
 */
inline bool ipc_reply(const uint32_t (&words)[CALL_WORDS]) {
	return register_call(SYS_IPC_REPLY, 0, words, CALL_NO_TIMEOUT).ok;
}

/** Ticket value returned by `ipc_defer` when no call is being served. */
constexpr uint32_t NO_TICKET = 0xFFFFFFFF;

/**
 * @brief Detaches the call being served so the server can accept others (`SYS_IPC_DEFER`).
 *
 * The caller stays blocked. Answer later with `ipc_reply_to` on the same thread. The
 * caller timeout and the death of the server still apply to the detached call.
 * @return The ticket (the id of the caller), or `NO_TICKET` when no call is being served.
 */
inline uint32_t ipc_defer() {
	uintptr_t result = syscall(SYS_IPC_DEFER, 0, 0, 0, 0, 0, 0);
	return result == FAILURE ? NO_TICKET : static_cast<uint32_t>(result);
}

/**
 * @brief Answers a call detached with `ipc_defer` (`SYS_IPC_REPLY_TO`). Does not block.
 * @param ticket Value returned by `ipc_defer`.
 * @param words The four answer words.
 * @return true when delivered; false when this thread holds no detached call with that
 *         ticket (wrong thread, already answered, caller timed out or died).
 */
inline bool ipc_reply_to(uint32_t ticket, const uint32_t (&words)[CALL_WORDS]) {
	return register_call(SYS_IPC_REPLY_TO, ticket, words, CALL_NO_TIMEOUT).ok;
}

/**
 * @brief Answers the call being served and waits for the next one (`SYS_IPC_REPLY_WAIT`). Blocks.
 *
 * With no call being served it only waits, like `ipc_accept`. The typical server is one
 * `ipc_accept` followed by a loop of `ipc_reply_wait`.
 * @param words The four answer words.
 * @param milliseconds Longest wait for the next call; `CALL_NO_TIMEOUT` by default.
 * @return The same results as `ipc_accept`.
 */
inline CallResult ipc_reply_wait(const uint32_t (&words)[CALL_WORDS],
								 uint32_t milliseconds = CALL_NO_TIMEOUT) {
	return register_call(SYS_IPC_REPLY_WAIT, 0, words, milliseconds);
}

} // namespace cock::user

#endif // !COCK_USER_CALL_HPP
