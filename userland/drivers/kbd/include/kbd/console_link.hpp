/** @file
 * @brief Sending the decoded keys to the tty server.
 */
#ifndef KBD_CONSOLE_LINK_HPP
#define KBD_CONSOLE_LINK_HPP

#include <stdint.h>

namespace cock::kbd {

/**
 * @brief Sends one key to the tty server as `CONSOLE_KEY`.
 *
 * Does nothing when the key has no character. When there is no handle on the tty server or the send
 * fails, connects to the server again through the name server and sends once more, so a tty
 * that was started again receives the keys; `console` then holds the handle the name server gave.
 *
 * @param console Handle on the endpoint of the tty server, or `NO_CAPABILITY_NODE` before the first key; updated
 * when it is connected again.
 * @param character The decoded character, or `NO_CHARACTER`.
 * @note A key is lost when no tty is running at that moment.
 */
void sendKey(uint32_t &console, char character);

/**
 * @brief Asks the `tty` server to switch to another terminal, as a `CONSOLE_KEY` with `arg1` set.
 *
 * Reconnects like `sendKey`.
 *
 * @param console Handle on the endpoint of the `tty` server, as in `sendKey`.
 * @param terminal The terminal number plus one, from `Decoder::takeSwitch`.
 */
void sendSwitch(uint32_t &console, uint32_t terminal);

} // namespace cock::kbd

#endif // !KBD_CONSOLE_LINK_HPP
