/** @file
 * @brief The asynchronous messages the pipe server receives and ignores.
 */
#ifndef PIPE_INBOX_HPP
#define PIPE_INBOX_HPP

namespace cock::pipe {

/**
 * @brief Takes every pending asynchronous message out of the inbox and drops it.
 *
 * The server is driven only by synchronous calls. A message in the inbox would make every accept return at once,
 * so it is removed.
 */
void discardMessages();

} // namespace cock::pipe

#endif // !PIPE_INBOX_HPP
