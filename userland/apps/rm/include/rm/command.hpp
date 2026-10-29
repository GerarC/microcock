/** @file
 * @brief `rm path...`: deletes files.
 */
#ifndef RM_COMMAND_HPP
#define RM_COMMAND_HPP

namespace cock::rm {

/**
 * @brief Deletes each file in order.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and at least one path.
 * @return 0 when every file was deleted; 1 after printing `rm: <path>: <reason>` for each one that failed; 2 after
 * printing `usage: rm path...` without arguments.
 */
int run(int argc, char **argv);

} // namespace cock::rm

#endif // !RM_COMMAND_HPP
