/** @file
 * @brief The rules that decide which program may connect to which: the `ipc=` lists of the manifest, and the owner and mode of each server.
 */
#ifndef NAMES_POLICY_HPP
#define NAMES_POLICY_HPP

#include <names/services.hpp>

namespace cock::names {

/**
 * @brief Tells whether the owner and the mode of a server let a client in.
 *
 * The owner of a server is the program whose label is `server.owner`, and every other program is "other", as in
 * the permission bits of a Unix file. The owner needs `MODE_OWNER` in the mode of the server, any other
 * program `MODE_OTHER`. A server with no owner has only "other" clients.
 * @param client The service the caller runs.
 * @param server The service it asks for.
 * @return true when the bit for the class of the client is set in `server.mode`.
 */
bool permits(const Service &client, const Service &server);

/**
 * @brief Tells whether a client may be given the right to talk to a server.
 *
 * The `ipc=` rule is the same as the one the kernel applies to a message, which is symmetric: it holds when the
 * client lists the label of the server, or the server lists the label of the client. A program that is not
 * in either list has no business with the other. The owner and the mode of the server must also allow the
 * client (`permits`).
 * @param client The service the caller runs.
 * @param server The service it asks for.
 * @return true when one of the two `ipc=` lists names the other and `permits` holds.
 */
bool mayConnect(const Service &client, const Service &server);

} // namespace cock::names

#endif // !NAMES_POLICY_HPP
