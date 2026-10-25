/** @file
 * @brief `uname`: prints the name and the version of the system.
 */
#ifndef UNAME_COMMAND_HPP
#define UNAME_COMMAND_HPP

namespace cock::uname {

/**
 * @brief Prints `microcock v<major>.<minor>.<patch>`.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and arguments; the arguments are ignored.
 * @return 0.
 */
int run(int argc, char **argv);

} // namespace cock::uname

#endif // !UNAME_COMMAND_HPP
