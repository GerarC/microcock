/** @file
 * @brief The synchronous calls a client makes to the `tty` server: read and attach.
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
 * `READ` answers with typed input of the terminal in the header, or defers the caller until a line is ready there.
 * `ATTACH` gives the caller the lowest terminal that has no living owner and answers with its number as the count.
 * Every other operation belongs to the console driver and is refused.
 *
 * @param sender Process id of the caller.
 * @param request The request words, never trusted.
 * @param reply Receives the answer words; zeroed first.
 * @return `DEFERRED` when the caller was put on hold, `REPLY` otherwise.
 * @note The status in the answer is `BAD_REQUEST` for another operation, a length of zero or above the chunk size or a
 * terminal that does not exist, and `BUSY` when a read cannot be deferred or every terminal has a living owner.
 */
Served serve(uint32_t sender, const uint32_t (&request)[ANSWER_WORDS], uint32_t (&reply)[ANSWER_WORDS]);

} // namespace cock::tty

#endif // !TTY_REQUESTS_HPP
