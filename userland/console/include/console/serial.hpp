/** @file
 * @brief The serial port the console server writes to.
 */
#ifndef CONSOLE_SERIAL_HPP
#define CONSOLE_SERIAL_HPP

#include <stddef.h>

namespace cock::console {

/**
 * @brief Takes the ports of the first serial line.
 *
 * Needs the port capability that `init` grants, so it must run after `CAPABILITY_GRANTED`.
 *
 * @return True when the ports are granted, false when the capability is missing.
 */
bool openSerial();

/**
 * @brief Sends bytes through the serial line, one at a time.
 *
 * Each byte waits a bounded number of polls for the transmitter to be empty and is sent even
 * when the wait runs out.
 *
 * @param text Bytes to send.
 * @param length Number of bytes.
 */
void serialWrite(const char *text, size_t length);

} // namespace cock::console

#endif // !CONSOLE_SERIAL_HPP
