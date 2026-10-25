/** @file
 * @brief Synchronous IPC: call, accept and reply between threads, with
 * deferred replies and time limits.
 *
 * A caller hands four words to a server and sleeps until the answer is written
 * straight into its saved registers. The server finds the caller id and the
 * words in its own result registers. Every state change runs with interrupts
 * blocked.
 */
#ifndef CORE_SYNC_IPC_HPP
#define CORE_SYNC_IPC_HPP

#include <cock/abi/call.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>

namespace cock::core::task {

/** @brief Outcome of SyncIpc::accept. */
enum class AcceptResult {
	SERVED, /**< A request was dequeued and written to the server registers. */
	BLOCKED, /**< Nothing to serve: the server sleeps, its result is written on
				wake up. */
	PENDING, /**< An asynchronous message or notification is waiting instead. */
	TIMED_OUT, /**< Nothing arrived and the time limit was zero. */
	REFUSED /**< The server is still serving a call. */
};

/** @brief Outcome of SyncIpc::call. */
enum class CallStart {
	BLOCKED, /**< The caller sleeps until the reply, a timeout or the death of
				the server. */
	TIMED_OUT, /**< The time limit was zero, nothing was sent. */
	REFUSED /**< The target does not exist, is dead or is the caller itself. */
};

/**
 * @brief Static operations of the synchronous call protocol.
 *
 * Queues are intrusive: the callers waiting on a server, the call it is
 * serving and the calls it deferred are linked through Thread fields, so no
 * allocation happens.
 */
class SyncIpc {
  public:
	/**
	 * @brief Starts a call to a thread.
	 *
	 * Queues the caller on the target. If the target is waiting in accept it is
	 * served at once and receives a scheduler handoff.
	 *
	 * @param caller Thread making the call.
	 * @param target_id Thread id of the server.
	 * @param words Four payload words copied into the caller.
	 * @param milliseconds Time limit, the no timeout value for forever, zero to
	 * refuse to wait.
	 * @param peer Value the server finds in its `peer` register when it accepts the
	 * call: the badge of the handle the caller used, or the id of the caller for a
	 * call by thread id.
	 * @return See CallStart.
	 * @note Syscall context. The caller is left in the waiting for reply state
	 * and a reschedule is requested.
	 */
	static CallStart call(Thread &caller, uint32_t target_id,
						  const uint32_t (&words)[abi::call::WORD_COUNT],
						  uint32_t milliseconds, uint32_t peer);
	/**
	 * @brief Starts a call to an endpoint.
	 *
	 * The call waits in the queue of the endpoint, not on a thread, so it survives the death of the
	 * server: whichever process is bound next accepts it. If the main thread of the bound process
	 * is waiting in accept it is served at once and receives a scheduler handoff.
	 *
	 * @param caller Thread making the call.
	 * @param endpoint Endpoint id.
	 * @param words Four payload words copied into the caller.
	 * @param milliseconds Time limit, the no timeout value for forever, zero to refuse to wait.
	 * @param peer Badge of the handle the caller used; the server finds it in its `peer` register.
	 * @return See CallStart. `REFUSED` when the endpoint does not exist or is bound to the process
	 * of the caller.
	 * @note Syscall context. The caller is left in the waiting for reply state and a reschedule is
	 * requested. With no timeout, a caller waits as long as no process is bound.
	 */
	static CallStart callThroughEndpoint(Thread &caller, uint32_t endpoint,
										 const uint32_t (&words)[abi::call::WORD_COUNT],
										 uint32_t milliseconds, uint32_t peer);
	/**
	 * @brief Takes the next request, or waits for one.
	 * @param server Thread serving.
	 * @param milliseconds Time limit, the no timeout value for forever, zero to
	 * poll.
	 * @return See AcceptResult. Message and notification arrivals wake a
	 * waiting server with the pending result.
	 */
	static AcceptResult accept(Thread &server, uint32_t milliseconds);
	/**
	 * @brief Answers the call being served.
	 * @param server Thread serving.
	 * @param words Four reply words written into the caller registers.
	 * @param handoff True to switch to the caller at once, false to just make
	 * it ready.
	 * @return True when a call was being served, false otherwise.
	 */
	static bool reply(Thread &server, const uint32_t (&words)[abi::call::WORD_COUNT],
					  bool handoff);
	/**
	 * @brief Detaches the call being served so it can be answered later.
	 * @param server Thread serving.
	 * @return The caller, whose id is the ticket, or nullptr when no call is
	 * being served.
	 */
	static Thread *defer(Thread &server);
	/**
	 * @brief Answers a deferred call.
	 * @param server Thread that deferred it.
	 * @param ticket Id of the caller, as returned by defer.
	 * @param words Four reply words.
	 * @return True when the ticket names a call deferred by this server.
	 */
	static bool replyTo(Thread &server, uint32_t ticket,
						const uint32_t (&words)[abi::call::WORD_COUNT]);
	/**
	 * @brief Tells whether requests are queued for a server.
	 * @param server Thread to test.
	 * @return True when at least one caller waits on the thread or, for the main thread of a user
	 * process, on an endpoint bound to its process.
	 */
	static bool hasCallers(const Thread &server);
	/**
	 * @brief Serves the first queued caller of a server that sleeps in accept.
	 * @param server Thread in the waiting for call state with a queued caller. It is made ready
	 * with the request in its registers.
	 */
	static void resume(Thread &server);
	/**
	 * @brief Fails every call tied to a thread that is dying.
	 * @param dying Thread being killed. Callers queued, deferred or being
	 * served get the failure result and are made ready.
	 */
	static void abandon(Thread &dying);
	/**
	 * @brief Takes a thread that waits for the reply of a call out of every list that points to it.
	 *
	 * The thread is removed from the queue of its endpoint, or from the lists of its server (the one
	 * being served, the callers and the deferred ones), and its call fields are cleared. It is not
	 * woken and gets no result: the caller decides whether it dies or starts again. A thread that
	 * waits for no reply is left as it is.
	 *
	 * @param caller Thread to take out; it is not the running thread.
	 */
	static void withdraw(Thread &caller);
	/**
	 * @brief Wakes a server sleeping in accept because a message or a
	 * notification arrived.
	 * @param server Thread in the waiting for call state. It resumes with the
	 * pending result.
	 */
	static void wakeForMessage(Thread &server);
	/**
	 * @brief Handles the expiry of a time limit of a synchronous wait.
	 * @param thread Thread waiting for a call or for a reply. A server gets the
	 * timed out result. A caller is removed from the queue it was in and gets
	 * the timed out result too.
	 * @note Called by Timeouts::onTick.
	 */
	static void expire(Thread &thread);

  private:
	/**
	 * @brief Finds the saved register frame of a thread.
	 * @param thread Any thread.
	 * @return The syscall frame for the running thread, else its saved stack
	 * pointer.
	 */
	static uintptr_t frameOf(Thread &thread);
	/**
	 * @brief Removes the first queued caller of a server from its queue: the callers of the thread
	 * first, then those of the endpoints bound to its process.
	 * @param server Server.
	 * @return The caller, or null when none waits.
	 */
	static Thread *takeCaller(Thread &server);
	/**
	 * @brief Takes a caller out of the queue of the endpoint it waits in.
	 * @param caller Thread whose `callEndpoint` is set. It is left with no endpoint.
	 * @return True when it was in the queue.
	 */
	static bool leaveEndpoint(Thread &caller);
	/**
	 * @brief Moves the first queued caller of a server into the serving slot
	 * and writes caller id and words into the server registers.
	 * @param server Server with at least one caller.
	 */
	static void dispatch(Thread &server);
	/**
	 * @brief Removes a thread from a call queue.
	 * @param head Head of the queue.
	 * @param thread Thread to remove.
	 * @return True when it was in the queue.
	 */
	static bool unlink(Thread *&head, Thread &thread);
	/**
	 * @brief Writes a successful reply into a caller and wakes it.
	 * @param server Thread replying.
	 * @param caller Thread being answered.
	 * @param words Reply words.
	 * @param handoff True to switch to the caller at once.
	 */
	static void deliver(Thread &server, Thread &caller,
						const uint32_t (&words)[abi::call::WORD_COUNT], bool handoff);
	/**
	 * @brief Ends a call with a bare result code and wakes the caller.
	 * @param caller Thread to finish.
	 * @param result Result code, such as failure or timed out. Words are zero.
	 */
	static void finish(Thread *caller, uintptr_t result);
	/**
	 * @brief Overwrites only the result register of a thread saved frame.
	 * @param thread Thread to patch.
	 * @param result New result code.
	 */
	static void writeResult(Thread &thread, uintptr_t result);
};

} // namespace cock::core::task

#endif // !CORE_SYNC_IPC_HPP
