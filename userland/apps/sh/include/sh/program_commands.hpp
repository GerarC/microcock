/** @file
 * @brief Running the programs of a line from the prompt: `fork`, descriptors, `exec` and `wait` through `pm`.
 */
#ifndef SH_PROGRAM_COMMANDS_HPP
#define SH_PROGRAM_COMMANDS_HPP

#include <cock/shell/pipeline.hpp>

namespace cock::sh {

/** @brief Status of a program that ended well. */
constexpr int STATUS_OK = 0;

/** @brief Status of a command whose program was not found, as every shell reports it. */
constexpr int STATUS_NOT_FOUND = 127;

/** @brief Status of a command whose file exists but cannot be run (a directory, an empty file, not a loadable image). */
constexpr int STATUS_NOT_EXECUTABLE = 126;

/** @brief Status of a command that could not be started: `pm` does not answer or refused the copy. */
constexpr int STATUS_NOT_STARTED = 125;

/**
 * @brief Runs the programs of a pipeline and waits for all of them to end.
 *
 * The first word of each command names its program: a name with no `/` is looked for in `/bin`; a name with one is a
 * path, relative to the current directory or absolute. For each command the shell asks `pm` for a copy of itself
 * (`fork`); the copy puts its descriptors in place and replaces its image with the file (`exec`, with the words as
 * arguments, the descriptors and the current directory it has). Between two commands the shell creates a pipe: the
 * output of the first command is the write end and the input of the next is the read end (`dup2` in the copies), and
 * the shell closes its own copies of both ends at once so the reader sees the end of the data when the writer ends. A redirection opens its file in the copy (`<` read only; `>` creates and truncates; `>>`
 * creates and appends) and makes it descriptor 0 or 1; when the file cannot be opened the copy prints
 * `sh: <file>: <reason>` and ends with status 1. A copy that cannot become the program prints why and ends with
 * `STATUS_NOT_FOUND` or `STATUS_NOT_EXECUTABLE`. The programs write on the same console unless redirected.
 *
 * @param pipeline The commands, as `cock::shell::parsePipeline` gives them: at least one, each with a program name.
 * @return The exit status of the last command. `STATUS_NOT_STARTED` after printing a message when a program path is
 * too long, `pm` could not be asked or refused a copy, or a pipe could not be created (it prints `sh: pipe: <reason>`); the commands already started are
 * still waited for. The value is also what `lastStatus` returns afterwards.
 */
int runPipeline(const cock::shell::Pipeline &pipeline);

/**
 * @brief Status of the last pipeline `runPipeline` ran.
 * @return The status, or 0 before any pipeline ran.
 */
int lastStatus();

} // namespace cock::sh

#endif // !SH_PROGRAM_COMMANDS_HPP
