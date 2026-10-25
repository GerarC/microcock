/** @file
 * @brief `cat path...`: prints files.
 */
#ifndef CAT_COMMAND_HPP
#define CAT_COMMAND_HPP

namespace cock::cat {

/**
 * @brief Prints each file in order, byte for byte.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and at least one path.
 * @return 0 when every file was printed; 1 when one failed (the others are still printed); 2 after printing
 * `usage: cat path...` without arguments.
 */
int run(int argc, char **argv);

} // namespace cock::cat

#endif // !CAT_COMMAND_HPP
