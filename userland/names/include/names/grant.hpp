/** @file
 * @brief Waiting for the capability `init` grants to the name server.
 */
#ifndef NAMES_GRANT_HPP
#define NAMES_GRANT_HPP

namespace cock::names {

/**
 * @brief Blocks until the first `CAPABILITY_GRANTED` message arrives.
 *
 * `init` sends it after it labelled the name server and gave it the right to pass on handles with
 * the badge of its choice; the badge of the message must be `ROOT_PID`. Synchronous calls that
 * arrive earlier wait in the queue of the kernel and are served afterwards, so nothing is lost;
 * any other asynchronous message is dropped.
 */
void waitForCapabilities();

} // namespace cock::names

#endif // !NAMES_GRANT_HPP
