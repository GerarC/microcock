/** @file
 * @brief Sending text and control requests to the display server (`vga_driver`).
 */
#ifndef CONSOLE_DISPLAY_HPP
#define CONSOLE_DISPLAY_HPP

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

namespace cock::console {

/**
 * @brief Connects to the process that draws on the screen, through the name server.
 *
 * Until a connection exists, every call of this module does nothing.
 *
 * @return true when the display server runs and the manifest lets the console talk to it.
 */
bool connectDisplay();

/**
 * @brief Draws text on the screen, in chunks the display server accepts.
 *
 * @param text Bytes to draw.
 * @param length Number of bytes.
 * @note A chunk is retried a few times when the inbox of the display server is full, and
 * dropped after that.
 */
void screenWrite(const char *text, size_t length);

/**
 * @brief Asks the display server to clear the screen.
 */
void screenClear();

/**
 * @brief Draws one character, for the echo of typed keys.
 *
 * @param character The character; newline and backspace have their usual effect.
 */
void screenCharacter(char character);

} // namespace cock::console

#endif // !CONSOLE_DISPLAY_HPP
