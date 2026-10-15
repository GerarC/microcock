#ifndef COCK_USER_CALL_HPP
#define COCK_USER_CALL_HPP

#include <cock/abi/call.hpp>
#include <cock/user/api.hpp>
#include <stdint.h>

namespace cock::user {

constexpr size_t CALL_WORDS = abi::call::WORD_COUNT;
constexpr uint32_t CALL_NO_TIMEOUT = abi::call::NO_TIMEOUT;

enum class CallStatus : uint32_t {
	OK = static_cast<uint32_t>(abi::call::SUCCESS),
	TIMED_OUT = static_cast<uint32_t>(abi::call::TIMED_OUT),
	MESSAGE_PENDING = static_cast<uint32_t>(abi::call::MESSAGE_PENDING),
	FAILED = static_cast<uint32_t>(abi::call::FAILURE),
};

struct CallResult {
	bool ok;
	CallStatus status;
	uint32_t peer;
	uint32_t words[CALL_WORDS];
};

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

inline CallResult ipc_call(uint32_t target, const uint32_t (&words)[CALL_WORDS],
						   uint32_t milliseconds = CALL_NO_TIMEOUT) {
	return register_call(SYS_IPC_CALL, target, words, milliseconds);
}

inline CallResult ipc_accept(uint32_t milliseconds = CALL_NO_TIMEOUT) {
	return register_call(SYS_IPC_ACCEPT, 0, nullptr, milliseconds);
}

inline bool ipc_reply(const uint32_t (&words)[CALL_WORDS]) {
	return register_call(SYS_IPC_REPLY, 0, words, CALL_NO_TIMEOUT).ok;
}

inline CallResult ipc_reply_wait(const uint32_t (&words)[CALL_WORDS],
								 uint32_t milliseconds = CALL_NO_TIMEOUT) {
	return register_call(SYS_IPC_REPLY_WAIT, 0, words, milliseconds);
}

} // namespace cock::user

#endif // !COCK_USER_CALL_HPP
