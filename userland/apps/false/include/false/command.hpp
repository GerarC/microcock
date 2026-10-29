/** @file
 * @brief `false`: does nothing and fails.
 */
#ifndef FALSE_COMMAND_HPP
#define FALSE_COMMAND_HPP

namespace cock::false_command {

/**
 * @brief Ends with a failure status, and prints nothing.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and arguments; the arguments are ignored.
 * @return 1, always.
 */
int run(int argc, char **argv);

} // namespace cock::false_command

#endif // !FALSE_COMMAND_HPP
