#include <cock/abi/call.hpp>
#include <cock/core/syscall/syscalls.hpp>
#include <cock/core/task/scheduler.hpp>
#include <cock/core/task/sync_ipc.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>

namespace cock::core::syscall {

using abi::call::FAILURE;
using abi::call::MESSAGE_PENDING;
using abi::call::SUCCESS;
using abi::call::TIMED_OUT;
using abi::call::WORD_COUNT;
using core::task::AcceptResult;
using core::task::CallStart;
using core::task::Scheduler;
using core::task::SyncIpc;
using core::task::Thread;

namespace {

struct Words {
	uint32_t values[WORD_COUNT];
};

Words pack(uintptr_t first, uintptr_t second, uintptr_t third, uintptr_t fourth) {
	return {{static_cast<uint32_t>(first), static_cast<uint32_t>(second),
			 static_cast<uint32_t>(third), static_cast<uint32_t>(fourth)}};
}

bool timeoutOk(uintptr_t milliseconds) {
	return milliseconds == abi::call::NO_TIMEOUT ||
		   milliseconds <= abi::call::MAX_TIMEOUT_MILLISECONDS;
}

uintptr_t acceptStatus(AcceptResult result) {
	switch (result) {
	case AcceptResult::SERVED:
	case AcceptResult::BLOCKED: return SUCCESS;
	case AcceptResult::PENDING: return MESSAGE_PENDING;
	case AcceptResult::TIMED_OUT: return TIMED_OUT;
	case AcceptResult::REFUSED: break;
	}
	return FAILURE;
}

} // namespace

uintptr_t sys_ipc_call(uintptr_t target, uintptr_t word0, uintptr_t word1,
					   uintptr_t word2, uintptr_t word3, uintptr_t milliseconds) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !timeoutOk(milliseconds)) return FAILURE;

	Words words = pack(word0, word1, word2, word3);
	switch (SyncIpc::call(*current, static_cast<uint32_t>(target), words.values,
						  static_cast<uint32_t>(milliseconds))) {
	case CallStart::BLOCKED: return SUCCESS;
	case CallStart::TIMED_OUT: return TIMED_OUT;
	case CallStart::REFUSED: break;
	}
	return FAILURE;
}

uintptr_t sys_ipc_accept(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
						 uintptr_t milliseconds) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !timeoutOk(milliseconds)) return FAILURE;
	return acceptStatus(SyncIpc::accept(*current, static_cast<uint32_t>(milliseconds)));
}

uintptr_t sys_ipc_reply(uintptr_t, uintptr_t word0, uintptr_t word1, uintptr_t word2,
						uintptr_t word3, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;

	Words words = pack(word0, word1, word2, word3);
	return SyncIpc::reply(*current, words.values, false) ? SUCCESS : FAILURE;
}

uintptr_t sys_ipc_defer(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;

	Thread *caller = SyncIpc::defer(*current);
	return caller != nullptr ? caller->getId() : FAILURE;
}

uintptr_t sys_ipc_reply_to(uintptr_t ticket, uintptr_t word0, uintptr_t word1,
						   uintptr_t word2, uintptr_t word3, uintptr_t) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current) return FAILURE;

	Words words = pack(word0, word1, word2, word3);
	return SyncIpc::replyTo(*current, static_cast<uint32_t>(ticket), words.values)
			   ? SUCCESS
			   : FAILURE;
}

uintptr_t sys_ipc_reply_wait(uintptr_t, uintptr_t word0, uintptr_t word1,
							 uintptr_t word2, uintptr_t word3, uintptr_t milliseconds) {
	Thread *current = Scheduler::getCurrentThread();
	if (!current || !timeoutOk(milliseconds)) return FAILURE;

	if (current->serving != nullptr) {
		Words words = pack(word0, word1, word2, word3);
		bool more = SyncIpc::hasCallers(*current);
		if (!SyncIpc::reply(*current, words.values, !more)) return FAILURE;
	}
	return acceptStatus(SyncIpc::accept(*current, static_cast<uint32_t>(milliseconds)));
}

} // namespace cock::core::syscall
