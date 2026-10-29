/** @file
 * @brief `true`: does nothing and succeeds.
 */
#ifndef TRUE_COMMAND_HPP
#define TRUE_COMMAND_HPP

namespace cock::true_command {

/**
 * @brief Ends with a successful status.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and arguments; the arguments are ignored.
 * @return 0, always.
 */
int run(int argc, char **argv);

} // namespace cock::true_command

#endif // !TRUE_COMMAND_HPP
