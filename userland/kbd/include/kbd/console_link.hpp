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
 * Does nothing when the key has no character. When there is no console process id or the send
 * fails, looks the server up again by name and sends once more, so a console that was started
 * again receives the keys; `console` then holds the new process id.
 *
 * @param console Process id of the console server, or `NO_PROCESS`; updated when it is found again.
 * @param character The decoded character, or `NO_CHARACTER`.
 * @note A key is lost when no console is running at that moment.
 */
void sendKey(uint32_t &console, char character);

} // namespace cock::kbd

#endif // !KBD_CONSOLE_LINK_HPP
