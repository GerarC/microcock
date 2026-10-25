/** @file
 * @brief The asynchronous messages the pipe server receives: the notices of copies made by `fork`.
 */
#ifndef PIPE_INBOX_HPP
#define PIPE_INBOX_HPP

namespace cock::pipe {

/**
 * @brief Takes every pending asynchronous message out of the inbox.
 *
 * A `PM_FORKED` message whose badge is the root server gives the copy the ends of its parent (`copyEnds`); any
 * other message is dropped. The server calls this before it serves each call, because a call is served before a
 * message when both wait, and the notice of a copy is always older than the first call of the copy or of its
 * parent: draining first is what makes the copy see the ends the parent had when it forked. A message left in
 * the inbox would also make every accept return at once.
 */
void takeMessages();

} // namespace cock::pipe

#endif // !PIPE_INBOX_HPP
