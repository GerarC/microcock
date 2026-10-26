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
 * @param terminal Terminal whose image receives the text.
 * @note A chunk is retried a few times when the inbox of the display server is full, and
 * dropped after that.
 */
void screenWrite(const char *text, size_t length, uint32_t terminal);

/**
 * @brief Asks the display server to clear the image of a terminal.
 *
 * @param terminal Terminal to clear.
 */
void screenClear(uint32_t terminal);

/**
 * @brief Asks the display server to put a terminal on the screen.
 *
 * @param terminal Terminal to show.
 */
void screenShow(uint32_t terminal);

/**
 * @brief Draws one character, for the echo of typed keys.
 *
 * @param character The character; newline and backspace have their usual effect.
 * @param terminal Terminal whose image receives it.
 */
void screenCharacter(char character, uint32_t terminal);

} // namespace cock::console

#endif // !CONSOLE_DISPLAY_HPP
