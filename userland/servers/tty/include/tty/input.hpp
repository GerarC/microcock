/** @file
 * @brief The typed input: keys from the keyboard driver and the line being edited.
 */
#ifndef TTY_INPUT_HPP
#define TTY_INPUT_HPP

#include <cock/ipc/message.hpp>

namespace cock::tty {

/**
 * @brief Handles one asynchronous message: a typed key, or a request to switch terminal.
 *
 * Only `CONSOLE_KEY` from the keyboard driver counts; the driver is found by name the first
 * time and again whenever another process sends a key. A key goes to the line discipline of the active terminal and
 * its echo to the console driver (`echo`); a notice with `arg1` set switches to terminal `arg1 - 1` when it exists.
 * Anything else is ignored.
 *
 * @param message Message received, never trusted.
 */
void handleMessage(const cock::ipc::Message &message);

/**
 * @brief Handles every asynchronous message waiting in the inbox, without blocking.
 */
void drainMessages();

} // namespace cock::tty

#endif // !TTY_INPUT_HPP
