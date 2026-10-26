/** @file
 * @brief `ps`: lists the processes `pm` knows.
 */
#ifndef PS_COMMAND_HPP
#define PS_COMMAND_HPP

namespace cock::ps {

/**
 * @brief Prints `PID PPID STAT COMMAND` and one row per process, in color; a process with no parent prints `-` as its
 * parent.
 *
 * It asks `pm` for the whole table at once (`SNAPSHOT`, in a shared memory object), so the rows are consistent with each
 * other and the names are complete. The colors are escape sequences that the screen shows and the serial line drops.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and arguments; the arguments are ignored.
 * @return 0 after the last row; 1 after printing `ps: pm does not answer` when `pm` cannot be reached or stops
 * answering.
 */
int run(int argc, char **argv);

} // namespace cock::ps

#endif // !PS_COMMAND_HPP
