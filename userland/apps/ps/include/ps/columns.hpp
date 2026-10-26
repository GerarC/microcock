/** @file
 * @brief The layout of the table `ps` prints: a line of titles and one line per process, in color.
 */
#ifndef PS_COLUMNS_HPP
#define PS_COLUMNS_HPP

#include <cock/abi/pm.hpp>

namespace cock::ps {

/**
 * @brief Prints the titles `PID PPID STAT COMMAND`, each number column right aligned to the width of its values, in
 * the color of the titles, and a newline.
 */
void printHeader();

/**
 * @brief Prints one process on its own line.
 *
 * The process id and the parent id are right aligned; a process with no parent shows `-` as its parent and a process with
 * no name shows `-` as its name. The state letter has the color of its state (`stateColor`) and the name is dimmed when
 * the process has exited.
 *
 * @param record One row of a snapshot of `pm`.
 */
void printRow(const cock::abi::pm::ProcessRecord &record);

} // namespace cock::ps

#endif // !PS_COLUMNS_HPP
