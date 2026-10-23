/** @file
 * @brief The command table of the shell and the way a line of input reaches a command.
 */
#ifndef SH_COMMANDS_HPP
#define SH_COMMANDS_HPP

namespace cock::sh {

/**
 * @brief Prints how a command is used, as `usage: <command> <arguments>`.
 *
 * @param command Name of the command, as typed.
 * @param arguments Text that describes the arguments, such as `path` or `from to`.
 */
void usage(const char *command, const char *arguments);

/**
 * @brief Splits a line into words and runs the built-in command that the first word names.
 *
 * An empty line does nothing. A line with more words than a command line holds prints
 * `sh: too many words`; an unknown name prints `sh: <name>: command not found`.
 *
 * @param line Line of input, terminated by zero. The split writes terminators into it.
 */
void run(char *line);

} // namespace cock::sh

#endif // !SH_COMMANDS_HPP
