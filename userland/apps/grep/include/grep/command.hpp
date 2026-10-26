/** @file
 * @brief `grep text [path...]`: prints the lines that contain a text.
 */
#ifndef GREP_COMMAND_HPP
#define GREP_COMMAND_HPP

namespace cock::grep {

/** @brief Exit status when at least one line matched. */
constexpr int STATUS_MATCHED = 0;

/** @brief Exit status when the inputs were read and no line matched. */
constexpr int STATUS_NO_MATCH = 1;

/** @brief Exit status when the arguments are wrong or an input could not be read; it wins over a match. */
constexpr int STATUS_ERROR = 2;

/**
 * @brief Prints every line that contains the text, as plain characters (no patterns, no options). With two or more
 * files each line starts with `<path>:`; with no path it reads the standard input.
 *
 * Only the first 255 bytes of a longer line are compared and printed.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name, the text and any number of paths. An empty text matches every line.
 * @return `STATUS_MATCHED`, `STATUS_NO_MATCH` or `STATUS_ERROR`; a missing text prints `usage: grep text [path...]`.
 */
int run(int argc, char **argv);

} // namespace cock::grep

#endif // !GREP_COMMAND_HPP
