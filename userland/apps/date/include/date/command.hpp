/** @file
 * @brief `date`: prints the date and time of the clock.
 */
#ifndef DATE_COMMAND_HPP
#define DATE_COMMAND_HPP

namespace cock::date {

/**
 * @brief Reads the clock from the `rtc` server and prints it as `YYYY-MM-DD HH:MM:SS` (UTC).
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and arguments; there must be none.
 * @return 0 after printing the time; 2 after printing `usage: date` when there are arguments; 1 after printing
 * `date: clock: <reason>` when the clock server cannot be reached or does not answer.
 */
int run(int argc, char **argv);

} // namespace cock::date

#endif // !DATE_COMMAND_HPP
