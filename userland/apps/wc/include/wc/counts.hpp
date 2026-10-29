/** @file
 * @brief The three counters of `wc`.
 */
#ifndef WC_COUNTS_HPP
#define WC_COUNTS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::wc {

/** @brief Counts of lines (newline characters), words (runs of characters that are not white space) and bytes. */
class Counts {
public:
	/**
	 * @brief Counts a chunk of the input. A word may continue from the previous chunk.
	 *
	 * @param bytes First byte.
	 * @param count Number of bytes.
	 */
	void add(const char *bytes, size_t count);

	/**
	 * @brief Adds the counters of another input, for the `total` line.
	 *
	 * @param other Counters of a finished input.
	 */
	void merge(const Counts &other);

	/** @brief Newline characters seen. */
	uint32_t lines = 0;

	/** @brief Words seen. */
	uint32_t words = 0;

	/** @brief Bytes seen. */
	uint32_t bytes = 0;

private:
	bool inWord = false;
};

} // namespace cock::wc

#endif // !WC_COUNTS_HPP
