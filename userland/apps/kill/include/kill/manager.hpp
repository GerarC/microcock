/** @file
 * @brief The connection of `kill` to the process manager (`pm`).
 */
#ifndef KILL_MANAGER_HPP
#define KILL_MANAGER_HPP

#include <cock/user/pm.hpp>

namespace cock::kill {

/**
 * @brief Connects to `pm` through the name server.
 *
 * @param client Receives the connection.
 * @return True when the name server gave a handle for `pm` and the client is ready; false otherwise.
 */
bool connectManager(cock::user::pm::Client &client);

} // namespace cock::kill

#endif // !KILL_MANAGER_HPP
