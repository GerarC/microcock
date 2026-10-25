/** @file
 * @brief `clear`: clears the screen.
 */
#ifndef CLEAR_COMMAND_HPP
#define CLEAR_COMMAND_HPP

namespace cock::clear {

/**
 * @brief Asks the console server to clear the screen.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and arguments; the arguments are ignored.
 * @return 0 when the server cleared it; 1 after printing `clear: the console server does not answer` otherwise.
 */
int run(int argc, char **argv);

} // namespace cock::clear

#endif // !CLEAR_COMMAND_HPP
