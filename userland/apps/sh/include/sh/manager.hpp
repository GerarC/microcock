/** @file
 * @brief The connection of the shell to the process manager (`pm`), used to run programs.
 */
#ifndef SH_MANAGER_HPP
#define SH_MANAGER_HPP

#include <cock/user/pm.hpp>

namespace cock::sh {

/**
 * @brief Gives the client of `pm`, connecting through the name server the first time.
 *
 * The client is kept for later calls. After a call on it failed, `forgetProcessManager` makes the next call connect
 * again.
 *
 * @return The connected client, or null when the name server does not give a handle for `pm`.
 */
cock::user::pm::Client *processManager();

/**
 * @brief Drops the connection, so the next `processManager` connects again.
 */
void forgetProcessManager();

} // namespace cock::sh

#endif // !SH_MANAGER_HPP
