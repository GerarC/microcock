/** @file
 * @brief `ps`: lists the processes `pm` knows.
 */
#ifndef PS_COMMAND_HPP
#define PS_COMMAND_HPP

namespace cock::ps {

/**
 * @brief Prints `PID PPID STAT` and one row per process; a process with no parent prints `-` as its parent.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and arguments; the arguments are ignored.
 * @return 0 after the last row; 1 after printing `ps: pm does not answer` when `pm` cannot be reached or stops
 * answering.
 */
int run(int argc, char **argv);

} // namespace cock::ps

#endif // !PS_COMMAND_HPP
