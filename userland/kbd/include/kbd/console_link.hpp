/** @file
 * @brief Sending the decoded keys to the console server.
 */
#ifndef KBD_CONSOLE_LINK_HPP
#define KBD_CONSOLE_LINK_HPP

#include <stdint.h>

namespace cock::kbd {

/**
 * @brief Sends one key to the console server as `CONSOLE_KEY`.
 *
 * Does nothing when there is no console server or the key has no character.
 *
 * @param console Process id of the console server, or `NO_PROCESS`.
 * @param character The decoded character, or `NO_CHARACTER`.
 * @note The send result is not checked: a console that died loses the key.
 */
void sendKey(uint32_t console, char character);

} // namespace cock::kbd

#endif // !KBD_CONSOLE_LINK_HPP
