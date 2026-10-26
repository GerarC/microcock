/** @file
 * @brief The echo of the typed line: what the `tty` server asks the console driver to draw.
 */
#ifndef TTY_OUTPUT_HPP
#define TTY_OUTPUT_HPP

#include <cock/abi/console_line.hpp>

namespace cock::tty {

/**
 * @brief Draws the echo that the line discipline asked for, through the console driver.
 *
 * Connects to the driver the first time and again after it was restarted. The echo goes to the screen only: the serial
 * port shows what programs print, not what was typed. A driver that does not answer loses the echo, never the input.
 *
 * @param kind What the line discipline returned for the character.
 * @param character The character that was typed.
 */
void echo(cock::abi::console::Echo kind, char character);

} // namespace cock::tty

#endif // !TTY_OUTPUT_HPP
