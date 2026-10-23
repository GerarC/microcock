/** @file
 * @brief The built-in commands that do not touch files.
 *
 * Every command takes the words of the line, where the first one is the name of the command,
 * and prints its result or its error on the console. None returns a status.
 */
#ifndef SH_BUILTINS_HPP
#define SH_BUILTINS_HPP

#include <cock/shell/words.hpp>

namespace cock::sh {

/**
 * @brief `version`: prints `microcock v<major>.<minor>.<patch>`.
 *
 * @param words Words of the line; unused.
 */
void versionCommand(const cock::shell::Words &words);

/**
 * @brief `echo text...`: prints the words joined by single spaces.
 *
 * @param words Words of the line.
 * @note Prints `sh: echo: text too long` when the text does not fit in `TEXT_BYTES`.
 */
void echoCommand(const cock::shell::Words &words);

/**
 * @brief `sleep seconds`: waits.
 *
 * @param words Words of the line.
 * @note Prints the usage when the count of words is wrong, the number is not valid or it is
 * above one hour.
 */
void sleepCommand(const cock::shell::Words &words);

/**
 * @brief `clear`: clears the screen through the console server.
 *
 * Connects to the console server the first time it is used.
 *
 * @param words Words of the line; unused.
 * @note Prints `sh: clear: the console server does not answer` when it fails.
 */
void clearCommand(const cock::shell::Words &words);

} // namespace cock::sh

#endif // !SH_BUILTINS_HPP
