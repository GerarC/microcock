/** @file
 * @brief The rule that decides which program may connect to which: the `ipc=` lists of the manifest.
 */
#ifndef NAMES_POLICY_HPP
#define NAMES_POLICY_HPP

#include <names/services.hpp>

namespace cock::names {

/**
 * @brief Tells whether a client may be given the right to talk to a server.
 *
 * The rule is the same as the one the kernel applies to a message, which is symmetric: it holds when the
 * client lists the label of the server, or the server lists the label of the client. A program that is not
 * in either list has no business with the other.
 * @param client The service the caller runs.
 * @param server The service it asks for.
 * @return true when one of the two `ipc=` lists names the other.
 */
bool mayConnect(const Service &client, const Service &server);

} // namespace cock::names

#endif // !NAMES_POLICY_HPP
