/** @file
 * @brief `echo text...`: prints its arguments.
 */
#ifndef ECHO_COMMAND_HPP
#define ECHO_COMMAND_HPP

namespace cock::echo {

/**
 * @brief Prints the arguments joined by single spaces, then a newline.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and arguments.
 * @return 0; with no arguments it prints an empty line.
 */
int run(int argc, char **argv);

} // namespace cock::echo

#endif // !ECHO_COMMAND_HPP
