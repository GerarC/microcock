/** @file
 * @brief The built-in commands that deal with the working directory of the shell.
 *
 * They are built-ins because the directory belongs to the process: a program the shell starts could not change it for
 * the shell. Every command takes the words of the line and prints its result or its error on the console.
 */
#ifndef SH_DIRECTORY_COMMANDS_HPP
#define SH_DIRECTORY_COMMANDS_HPP

#include <cock/shell/words.hpp>

namespace cock::sh {

/**
 * @brief `cd [path]`: changes the working directory; without a path it goes to `/`.
 *
 * @param words Words of the line.
 * @note Prints the usage when there is more than one argument, and `sh: cd: <path>: <reason>` when the directory cannot
 * be entered (`not found`, `not a directory`, `path too long`, or `failed`); the directory is then unchanged.
 */
void changeDirectoryCommand(const cock::shell::Words &words);

/**
 * @brief `pwd`: prints the working directory.
 *
 * @param words Words of the line; unused.
 */
void printDirectoryCommand(const cock::shell::Words &words);

} // namespace cock::sh

#endif // !SH_DIRECTORY_COMMANDS_HPP
