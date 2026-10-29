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
 * @brief Expands the variables of a line (`expandLine`), splits it into words and operators, groups them into the
 * commands of a pipeline (`cock::shell::parsePipeline`) and runs them: the built-in command that the first word
 * names, or else the programs (`runPipeline`). A line of `NAME=value` words only sets variables.
 *
 * An empty line does nothing. A line with more words than a command line holds prints
 * `sh: too many words`, and a line with a quote that is never closed prints `sh: unterminated quote`; a line that is
 * not a pipeline prints `sh: missing command`, `sh: missing file name after a redirection`,
 * `sh: too many commands in a pipeline` or `sh: input can only be redirected on the first command and output on the
 * last`. A built-in command in a pipeline or with a redirection prints
 * `sh: <name>: a built-in command cannot be piped or redirected`. The last program that ends with a status other
 * than 0 and the three the shell reports itself prints `sh: <name>: exit status <n>`.
 *
 * @param line Line of input, terminated by zero. The split writes terminators into it.
 */
void run(char *line);

} // namespace cock::sh

#endif // !SH_COMMANDS_HPP
