/** @file
 * @brief The serial port the console server writes to.
 */
#ifndef CONSOLE_SERIAL_HPP
#define CONSOLE_SERIAL_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::console {

/**
 * @brief Takes the ports of the first serial line.
 *
 * Needs the port capability that `rs` grants, so it must run after `CAPABILITY_GRANTED`.
 *
 * @return True when the ports are granted, false when the capability is missing.
 */
bool openSerial();

/**
 * @brief Sends bytes through the serial line, one at a time.
 *
 * Each byte waits a bounded number of polls for the transmitter to be empty and is sent even
 * when the wait runs out. The color sequences (`ESC [ ... m`, see `cock/abi/ansi.hpp`) are read and not sent, so the
 * serial log holds plain text; each terminal has its own reader, because its sequences may arrive in pieces.
 *
 * @param text Bytes to send.
 * @param length Number of bytes.
 * @param terminal Terminal the text is for; one out of range is read as the first.
 */
void serialWrite(const char *text, size_t length, uint32_t terminal);

} // namespace cock::console

#endif // !CONSOLE_SERIAL_HPP
