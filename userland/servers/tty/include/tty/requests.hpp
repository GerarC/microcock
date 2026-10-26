/** @file
 * @brief The synchronous calls a client makes to the `tty` server: read.
 */
#ifndef TTY_REQUESTS_HPP
#define TTY_REQUESTS_HPP

#include <stdint.h>
#include <tty/reply.hpp>

namespace cock::tty {

/** @brief What `serve` did with a call. */
enum class Served {
	REPLY, /**< The answer is ready and must be sent now. */
	DEFERRED, /**< The caller waits; the answer is sent later. */
};

/**
 * @brief Serves one call.
 *
 * `READ` answers with typed input, or defers the caller until a line is ready. Every other operation belongs to the
 * console driver and is refused.
 *
 * @param request The request words, never trusted.
 * @param reply Receives the answer words; zeroed first.
 * @return `DEFERRED` when the caller was put on hold, `REPLY` otherwise.
 * @note The status in the answer is `BAD_REQUEST` for an operation other than `READ` or a length of zero or above the
 * chunk size, and `BUSY` when a read cannot be deferred.
 */
Served serve(const uint32_t (&request)[ANSWER_WORDS], uint32_t (&reply)[ANSWER_WORDS]);

} // namespace cock::tty

#endif // !TTY_REQUESTS_HPP
