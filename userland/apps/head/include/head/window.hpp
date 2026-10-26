/** @file
 * @brief The part of a stream that `head` prints.
 */
#ifndef HEAD_WINDOW_HPP
#define HEAD_WINDOW_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::head {

/** @brief Counts newlines in the chunks of one input and tells how many bytes belong to the first lines. */
class Window {
public:
	/**
	 * @brief Starts a window.
	 *
	 * @param lines How many lines it covers; 0 covers nothing.
	 */
	explicit Window(uint32_t lines) : remaining(lines) {}

	/**
	 * @brief Takes the next chunk of the input.
	 *
	 * @param chunk First byte.
	 * @param count Number of bytes.
	 * @return How many bytes from the start of the chunk are inside the window: all of them while lines are still
	 * missing, up to and including the last newline when the window closes here.
	 */
	size_t take(const char *chunk, size_t count);

	/** @brief True once the last line of the window has been taken; the rest of the input is not needed. */
	bool closed() const { return remaining == 0; }

private:
	uint32_t remaining;
};

} // namespace cock::head

#endif // !HEAD_WINDOW_HPP
