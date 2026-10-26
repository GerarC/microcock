/** @file
 * @brief The synchronous calls a client makes to the console driver: write, clear, echo and show.
 */
#ifndef CONSOLE_REQUESTS_HPP
#define CONSOLE_REQUESTS_HPP

#include <console/reply.hpp>
#include <stdint.h>

namespace cock::console {

/**
 * @brief Serves one call; the answer is always ready at once.
 *
 * `WRITE` adds the chunk to the text of the sender, `CLEAR` clears the screen and `ECHO` draws the typed characters on
 * the screen only and `SHOW` puts a terminal on the screen, both for the `tty` server; each is about the terminal in
 * the header. `READ` and `ATTACH` belong to the `tty` server and are refused.
 *
 * @param sender Process id of the caller.
 * @param request The request words, never trusted.
 * @param reply Receives the answer words; zeroed first.
 * @note The status in the answer is `BAD_REQUEST` for an unknown operation, `READ` or a length of zero or above the
 * chunk size or a terminal that does not exist, and `DENIED` for an `ECHO` or `SHOW` from a process that is not the `tty`
 * server.
 */
void serve(uint32_t sender, const uint32_t (&request)[ANSWER_WORDS], uint32_t (&reply)[ANSWER_WORDS]);

} // namespace cock::console

#endif // !CONSOLE_REQUESTS_HPP
