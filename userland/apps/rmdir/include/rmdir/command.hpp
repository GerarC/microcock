/** @file
 * @brief `rmdir path...`: removes empty directories.
 */
#ifndef RMDIR_COMMAND_HPP
#define RMDIR_COMMAND_HPP

namespace cock::rmdir {

/**
 * @brief Removes each directory in order.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and at least one path.
 * @return 0 when every directory was removed; 1 after printing `rmdir: <path>: <reason>` for each one that failed
 * (`not empty`, `not found`, ...); 2 after printing `usage: rmdir path...` without arguments.
 */
int run(int argc, char **argv);

} // namespace cock::rmdir

#endif // !RMDIR_COMMAND_HPP
