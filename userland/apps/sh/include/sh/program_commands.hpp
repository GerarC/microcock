/** @file
 * @brief Running a program of the file system from the prompt: `fork`, `exec` and `wait` through `pm`.
 */
#ifndef SH_PROGRAM_COMMANDS_HPP
#define SH_PROGRAM_COMMANDS_HPP

#include <cock/shell/words.hpp>

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
 * @brief Runs the program that the first word names and waits for it to end.
 *
 * A name with no `/` is looked for in `/bin`; a name with one is a path, relative to the current directory or absolute.
 * The shell asks `pm` for a copy of itself (`fork`), the copy replaces its image with the file (`exec`, with the words
 * as arguments, the descriptors and the current directory the shell has), and the shell collects the status of the copy
 * (`wait`). The program writes on the same console. A copy that cannot become the program prints why and ends with
 * `STATUS_NOT_FOUND` or `STATUS_NOT_EXECUTABLE`.
 *
 * @param words Words of the line; the first names the program and all of them become its arguments.
 * @return The exit status of the program. `STATUS_NOT_STARTED` after printing `sh: <name>: cannot start the program`
 * when `pm` could not be asked or refused. The value is also what `lastStatus` returns afterwards.
 */
int runProgram(const cock::shell::Words &words);

/**
 * @brief Status of the last program `runProgram` ran.
 * @return The status, or 0 before any program ran.
 */
int lastStatus();

} // namespace cock::sh

#endif // !SH_PROGRAM_COMMANDS_HPP
