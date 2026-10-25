/** @file
 * @brief The built-in `exit`, which ends the shell.
 */
#ifndef SH_EXIT_COMMAND_HPP
#define SH_EXIT_COMMAND_HPP

#include <cock/shell/words.hpp>

namespace cock::sh {

/**
 * @brief `exit [status]`: ends the shell with the status given, or with the status of the last program.
 *
 * @param words Words of the line.
 * @note Does not return when the arguments are right. A status that is not a number, or more than one argument, prints
 * the usage and the shell goes on.
 */
void exitCommand(const cock::shell::Words &words);

} // namespace cock::sh

#endif // !SH_EXIT_COMMAND_HPP
