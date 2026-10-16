#ifndef CORE_SYNC_IPC_HPP
#define CORE_SYNC_IPC_HPP

#include <cock/abi/call.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>

namespace cock::core::task {

enum class AcceptResult { SERVED, BLOCKED, PENDING, TIMED_OUT, REFUSED };
enum class CallStart { BLOCKED, TIMED_OUT, REFUSED };

class SyncIpc {
  public:
	static CallStart call(Thread &caller, uint32_t target_id,
						  const uint32_t (&words)[abi::call::WORD_COUNT],
						  uint32_t milliseconds);
	static AcceptResult accept(Thread &server, uint32_t milliseconds);
	static bool reply(Thread &server, const uint32_t (&words)[abi::call::WORD_COUNT],
					  bool handoff);
	static Thread *defer(Thread &server);
	static bool replyTo(Thread &server, uint32_t ticket,
						const uint32_t (&words)[abi::call::WORD_COUNT]);
	static bool hasCallers(const Thread &server) { return server.callers != nullptr; }
	static void abandon(Thread &dying);
	static void wakeForMessage(Thread &server);
	static void expire(Thread &thread);

  private:
	static uintptr_t frameOf(Thread &thread);
	static void dispatch(Thread &server);
	static bool unlink(Thread *&head, Thread &thread);
	static void deliver(Thread &server, Thread &caller,
						const uint32_t (&words)[abi::call::WORD_COUNT], bool handoff);
	static void finish(Thread *caller, uintptr_t result);
	static void writeResult(Thread &thread, uintptr_t result);
};

} // namespace cock::core::task

#endif // !CORE_SYNC_IPC_HPP
