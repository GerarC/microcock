/** @file
 * @brief `wc [path...]`: counts lines, words and bytes.
 */
#ifndef WC_COMMAND_HPP
#define WC_COMMAND_HPP

namespace cock::wc {

/**
 * @brief Prints `<lines> <words> <bytes> <path>` for each file, and a `total` line when there are two or more; with no
 * path, counts the standard input and prints no name.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and any number of paths.
 * @return 0 when every input was counted; 1 when a file or the input failed (the other files are still counted).
 */
int run(int argc, char **argv);

} // namespace cock::wc

#endif // !WC_COMMAND_HPP
