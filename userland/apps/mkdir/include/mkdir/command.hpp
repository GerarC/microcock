/** @file
 * @brief `mkdir path...`: makes directories.
 */
#ifndef MKDIR_COMMAND_HPP
#define MKDIR_COMMAND_HPP

namespace cock::mkdir {

/**
 * @brief Makes each directory in order; the parent of each must exist.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and at least one path.
 * @return 0 when every directory was made; 1 after printing `mkdir: <path>: <reason>` for each one that failed (the
 * others are still made); 2 after printing `usage: mkdir path...` without arguments.
 */
int run(int argc, char **argv);

} // namespace cock::mkdir

#endif // !MKDIR_COMMAND_HPP
