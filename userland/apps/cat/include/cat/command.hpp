/** @file
 * @brief `cat [path...]`: prints files, or the standard input when there is no path.
 */
#ifndef CAT_COMMAND_HPP
#define CAT_COMMAND_HPP

namespace cock::cat {

/**
 * @brief Prints each file in order, byte for byte; with no path, copies the standard input until it ends.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and any number of paths.
 * @return 0 when everything was printed; 1 when a file or the input failed (the other files are still printed).
 */
int run(int argc, char **argv);

} // namespace cock::cat

#endif // !CAT_COMMAND_HPP
