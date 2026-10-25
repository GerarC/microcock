/** @file
 * @brief The connection of `ps` to the process manager (`pm`).
 */
#ifndef PS_MANAGER_HPP
#define PS_MANAGER_HPP

#include <cock/user/pm.hpp>

namespace cock::ps {

/**
 * @brief Connects to `pm` through the name server.
 *
 * @param client Receives the connection.
 * @return True when the name server gave a handle for `pm` and the client is ready; false otherwise.
 */
bool connectManager(cock::user::pm::Client &client);

} // namespace cock::ps

#endif // !PS_MANAGER_HPP
