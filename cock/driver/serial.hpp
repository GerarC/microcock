#ifndef CORE_SERIAL_HPP
#define CORE_SERIAL_HPP

#include <stddef.h>

namespace cock::driver {

/**
 * @brief Driver for the standard COM1 Serial Port.
 * * Used for headless debugging and logging kernel output to the host machine.
 */
class Serial {
  public:
	/**
	 * @brief Initializes the COM1 serial port (baud rate, parity, FIFO, IRQ).
	 */
	static void init();

	/**
	 * @brief Transmits a single character through the serial port.
	 * @param c The character to send.
	 */
	static void putChar(char c);

	/**
	 * @brief Transmits an array of characters.
	 * @param data Pointer to the buffer.
	 * @param size Number of bytes to write.
	 */
	static void write(const char *data, size_t size);

	/**
	 * @brief Transmits a null-terminated C-string.
	 * @param data Pointer to the string.
	 */
	static void writeString(const char *data);

  private:
	/**
	 * @brief Checks the serial line status register to see if the transmit
	 * buffer is empty.
	 * @return Non-zero if empty, 0 otherwise.
	 */
	static int isTransmitEmpty();
};

} // namespace cock::driver

#endif // !CORE_SERIAL_HPP
