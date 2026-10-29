/** @file
 * @brief The command line of `head`.
 */
#ifndef HEAD_OPTIONS_HPP
#define HEAD_OPTIONS_HPP

#include <stdint.h>

namespace cock::head {

/** @brief Lines printed when there is no `-n`. */
constexpr uint32_t DEFAULT_LINES = 10;

/** @brief What the arguments ask for. */
struct Options {
	/** Lines to print from each input. */
	uint32_t lines = DEFAULT_LINES;
	/** Index in `argv` of the first path; equal to `argc` when there is none. */
	int firstPath = 0;
};

/**
 * @brief Reads `[-n lines]` from the front of the arguments.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name, the option and the paths.
 * @param options Receives the line count and where the paths start.
 * @return True when the option is absent or complete and valid; false when `-n` has no count or the count is not a
 * decimal number.
 */
bool parseOptions(int argc, char **argv, Options &options);

} // namespace cock::head

#endif // !HEAD_OPTIONS_HPP
