/** @file
 * @brief The typed input: keys from the keyboard driver and the line being edited.
 */
#ifndef TTY_INPUT_HPP
#define TTY_INPUT_HPP

#include <cock/abi/console_line.hpp>
#include <cock/ipc/message.hpp>

namespace cock::tty {

/** @brief The line discipline: edits the typed line and queues the finished lines. */
extern cock::abi::console::LineDiscipline discipline;

/**
 * @brief Handles one asynchronous message: a typed key.
 *
 * Only `CONSOLE_KEY` from the keyboard driver counts; the driver is found by name the first
 * time and again whenever another process sends a key. The key goes to the line discipline
 * and its echo to the console driver (`echo`). Anything else is ignored.
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
