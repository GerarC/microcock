/** @file
 * @brief The variables of the shell: `$?`, `$PWD` and the ones set with `NAME=value`.
 */
#ifndef SH_VARIABLES_HPP
#define SH_VARIABLES_HPP

#include <cock/shell/words.hpp>

namespace cock::sh {

/**
 * @brief Replaces the variable references of a line with their values.
 *
 * `$?` is the status of the last program (`lastStatus`), `$PWD` the current directory (`getcwd`), and any other name
 * is a variable set with `NAME=value`; a name that is not set gives an empty text. See `cock::shell::expand` for the
 * quoting rules.
 *
 * @param line Null terminated line as typed.
 * @param out Receives the expanded line.
 * @return True on success. False after printing `sh: line too long` when the result does not fit a command line.
 */
bool expandLine(const char *line, char (&out)[cock::shell::MAX_LINE_BYTES]);

/**
 * @brief Runs the line as assignments when every word is `NAME=value`.
 *
 * Each assignment sets or replaces a variable. `PWD=x` creates a variable that `$PWD` never shows, because `$PWD`
 * always gives the current directory.
 *
 * @param words Words of the line, expanded and split.
 * @return True when the line was made of assignments only (it is done, also when one failed after printing
 *         `sh: <name>: too long` or `sh: too many variables`); false when some word is not an assignment and nothing
 *         was set.
 */
bool assignVariables(const cock::shell::Words &words);

/**
 * @brief The built-in `unset name...`: removes the variables that the arguments name.
 *
 * A name that is not set is ignored. With no arguments it prints `usage: unset name...`.
 *
 * @param words Words of the line.
 */
void unsetCommand(const cock::shell::Words &words);

/**
 * @brief The built-in `set`: prints the variables set with `NAME=value`, one `NAME=value` per line.
 *
 * @param words Words of the line; ignored.
 */
void setCommand(const cock::shell::Words &words);

} // namespace cock::sh

#endif // !SH_VARIABLES_HPP
