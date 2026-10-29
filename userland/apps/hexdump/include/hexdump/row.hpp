/** @file
 * @brief One row of the dump.
 */
#ifndef HEXDUMP_ROW_HPP
#define HEXDUMP_ROW_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::hexdump {

/** @brief Bytes shown in each row. */
constexpr size_t ROW_BYTES = 16;

/** @brief Hexadecimal digits of the offset at the start of a row. */
constexpr size_t OFFSET_DIGITS = 8;

/** @brief Collects bytes into rows and prints each row as soon as it is full. */
class RowWriter {
public:
	/**
	 * @brief Adds one byte; a full row is printed.
	 *
	 * @param byte The next byte of the input.
	 * @return False when the row could not be written to the standard output (`errno` has the reason); true otherwise.
	 */
	bool add(uint8_t byte);

	/**
	 * @brief Prints the row that is not full, padded, and then the final line with the total size.
	 *
	 * @return False when the output could not be written; true otherwise. An empty input prints nothing.
	 */
	bool finish();

private:
	bool flush();

	uint8_t pending[ROW_BYTES] = {};
	size_t filled = 0;
	uint32_t offset = 0;
};

} // namespace cock::hexdump

#endif // !HEXDUMP_ROW_HPP
