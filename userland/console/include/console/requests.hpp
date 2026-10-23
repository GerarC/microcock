/** @file
 * @brief The synchronous calls a client makes to the console server: write, read and clear.
 */
#ifndef CONSOLE_REQUESTS_HPP
#define CONSOLE_REQUESTS_HPP

#include <console/reply.hpp>
#include <stdint.h>

namespace cock::console {

/** @brief What `serve` did with a call. */
enum class Served {
	REPLY, /**< The answer is ready and must be sent now. */
	DEFERRED, /**< The caller waits; the answer is sent later. */
};

/**
 * @brief Serves one call.
 *
 * `WRITE` adds the chunk to the text of the sender, `CLEAR` clears the screen and `READ`
 * answers with typed input, or defers the caller until a line is ready.
 *
 * @param sender Process id of the caller.
 * @param request The request words, never trusted.
 * @param reply Receives the answer words; zeroed first.
 * @return `DEFERRED` when the caller was put on hold, `REPLY` otherwise.
 * @note The status in the answer is `BAD_REQUEST` for an unknown operation or a length of
 * zero or above the chunk size, and `BUSY` when a read cannot be deferred.
 */
Served serve(uint32_t sender, const uint32_t (&request)[ANSWER_WORDS],
			 uint32_t (&reply)[ANSWER_WORDS]);

} // namespace cock::console

#endif // !CONSOLE_REQUESTS_HPP
