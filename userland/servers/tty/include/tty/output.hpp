/** @file
 * @brief What the `tty` server asks the console driver to draw: the echo of the typed line and the terminal shown.
 */
#ifndef TTY_OUTPUT_HPP
#define TTY_OUTPUT_HPP

#include <cock/abi/console_line.hpp>
#include <stdint.h>

namespace cock::tty {

/**
 * @brief Draws the echo that the line discipline asked for, through the console driver.
 *
 * Connects to the driver the first time and again after it was restarted. The echo goes to the screen only: the serial
 * port shows what programs print, not what was typed. A driver that does not answer loses the echo, never the input.
 *
 * @param kind What the line discipline returned for the character.
 * @param character The character that was typed.
 * @param terminal Terminal whose image receives the echo.
 */
void echo(cock::abi::console::Echo kind, char character, uint32_t terminal);

/**
 * @brief Asks the console driver to put a terminal on the screen.
 *
 * @param terminal Terminal number, below `TERMINAL_COUNT`. A driver that does not answer leaves the screen as it was.
 */
void show(uint32_t terminal);

} // namespace cock::tty

#endif // !TTY_OUTPUT_HPP
