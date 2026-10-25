/** @file
 * @brief The built-in commands that ask the process manager (`pm`) about processes.
 *
 * Every command takes the words of the line, where the first one is the name of the command.
 * The first one that needs `pm` connects to it through the name server. None returns a status.
 */
#ifndef SH_PROCESS_COMMANDS_HPP
#define SH_PROCESS_COMMANDS_HPP

#include <cock/shell/words.hpp>

namespace cock::sh {

/**
 * @brief `ps`: prints the table of `pm`, one line per process: `PID PPID STAT`.
 *
 * `STAT` is `R` for a process that runs as far as `pm` knows and `X` for one whose exit notice arrived.
 * The parent of a process with none is printed as `-`.
 *
 * @param words Words of the line; unused.
 * @note Prints `sh: ps: pm does not answer` when `pm` cannot be reached or a call fails.
 */
void processListCommand(const cock::shell::Words &words);

} // namespace cock::sh

#endif // !SH_PROCESS_COMMANDS_HPP
