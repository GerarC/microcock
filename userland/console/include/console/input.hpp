/** @file
 * @brief The typed input: keys from the keyboard driver, the line being edited and its echo.
 */
#ifndef CONSOLE_INPUT_HPP
#define CONSOLE_INPUT_HPP

#include <cock/abi/console_line.hpp>
#include <cock/ipc/message.hpp>

namespace cock::console {

/** @brief The line discipline: edits the typed line and queues the finished lines. */
extern cock::abi::console::LineDiscipline discipline;

/**
 * @brief Handles one asynchronous message: a typed key.
 *
 * Only `CONSOLE_KEY` from the keyboard driver counts; the driver is found by name the first
 * time and again whenever another process sends a key. The key goes to the line discipline
 * and its echo to the screen. Anything else is ignored.
 *
 * @param message Message received, never trusted.
 */
void handleMessage(const cock::ipc::Message &message);

/**
 * @brief Handles every asynchronous message waiting in the inbox, without blocking.
 */
void drainMessages();

} // namespace cock::console

#endif // !CONSOLE_INPUT_HPP
