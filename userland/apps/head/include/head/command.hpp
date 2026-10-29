/** @file
 * @brief `head [-n lines] [path...]`: prints the first lines.
 */
#ifndef HEAD_COMMAND_HPP
#define HEAD_COMMAND_HPP

namespace cock::head {

/**
 * @brief Prints the first lines of each file, or of the standard input when there is no path. With two or more files
 * each one is preceded by `==> <path> <==`.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name, an optional `-n` and its count (10 lines by default) and any number of paths.
 * @return 0 when everything asked was printed; 1 when a file or the input failed (the other files are still printed);
 * 2 after printing `usage: head [-n lines] [path...]` when the option is malformed.
 */
int run(int argc, char **argv);

} // namespace cock::head

#endif // !HEAD_COMMAND_HPP
