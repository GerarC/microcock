/** @file
 * @brief The calls a client makes to the pipe server: create, read, write and close.
 */
#ifndef PIPE_REQUESTS_HPP
#define PIPE_REQUESTS_HPP

#include <pipe/reply.hpp>
#include <stdint.h>

namespace cock::pipe {

/** @brief What `serve` did with a call. */
enum class Served {
	REPLY, /**< The answer is ready and must be sent now. */
	DEFERRED, /**< The caller waits; the answer is sent later, when a pipe changes. */
};

/** @brief What `attempt` did with a `READ` or a `WRITE`. */
enum class Attempt {
	DONE, /**< The answer is ready, whether it is data, an end of file or an error. */
	BLOCKED, /**< The pipe is empty (for a read) or full (for a write) and can still change. */
};

/**
 * @brief Tries a `READ` or a `WRITE` once.
 *
 * A read takes the oldest bytes, or answers a count of zero when the pipe is empty and no write end is open. A
 * write puts as many bytes as fit and answers how many, or `BROKEN` when no read end is open. Everything else a
 * request can get wrong is answered here too.
 *
 * @param sender Process id of the caller, the badge of its handle.
 * @param request The request words, never trusted.
 * @param reply Receives the answer words, zeroed first.
 * @return `BLOCKED` when the pipe cannot serve it yet, so the caller must wait; `DONE` otherwise.
 */
Attempt attempt(uint32_t sender, const uint32_t (&request)[CALL_WORDS], uint32_t (&reply)[CALL_WORDS]);

/**
 * @brief Serves one call.
 *
 * `CREATE` and `CLOSE` are answered at once. `READ` and `WRITE` go through `attempt`, and a call that is
 * `BLOCKED` is put on hold (`holdCall`) and answered by `retryWaiters` when a pipe changes.
 *
 * @param sender Process id of the caller.
 * @param request The request words, never trusted.
 * @param reply Receives the answer words; zeroed first.
 * @return `DEFERRED` when the caller was put on hold, `REPLY` otherwise.
 * @note The answer is `NO_ROOM` when a pipe or two ends cannot be made, and `BUSY` when a call cannot be held.
 */
Served serve(uint32_t sender, const uint32_t (&request)[CALL_WORDS], uint32_t (&reply)[CALL_WORDS]);

} // namespace cock::pipe

#endif // !PIPE_REQUESTS_HPP
