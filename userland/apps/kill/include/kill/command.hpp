/** @file
 * @brief `kill [-signal] pid...`: sends a signal to processes through the process manager.
 */
#ifndef KILL_COMMAND_HPP
#define KILL_COMMAND_HPP

namespace cock::kill {

/**
 * @brief Sends one signal to each process; the signal is `TERM` unless the first argument is `-<signal>`.
 *
 * The signal is a name (`INT`, `KILL`, `TERM`, `CONT`, `STOP`) or the number of one of them (2, 9, 15, 18, 19). A pid
 * is the number the `PID` column of `ps` shows. Whether the caller may signal a process is decided by `pm`.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name, an optional signal and one or more pids.
 * @return 0 when every signal was sent; 1 after printing `kill: <pid>: <reason>` for each one that was not (the others
 * are still sent); 2 after printing `usage: kill [-signal] pid...` for a missing pid, a pid that is not a number or a
 * signal that is not one of the five.
 */
int run(int argc, char **argv);

} // namespace cock::kill

#endif // !KILL_COMMAND_HPP
