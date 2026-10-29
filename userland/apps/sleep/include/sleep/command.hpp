/** @file
 * @brief `sleep seconds`: waits.
 */
#ifndef SLEEP_COMMAND_HPP
#define SLEEP_COMMAND_HPP

#include <stdint.h>

namespace cock::sleep {

/** @brief Most seconds a call may ask for. */
constexpr uint32_t MAX_SECONDS = 3600;

/** @brief Milliseconds in a second. */
constexpr uint32_t MILLISECONDS_PER_SECOND = 1000;

/**
 * @brief Waits the given number of seconds.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and one argument: a whole number of seconds up to `MAX_SECONDS`.
 * @return 0 after the wait; 2 after printing `usage: sleep seconds` when the argument is missing, not a number or too
 * big; 1 when the kernel refused the wait.
 */
int run(int argc, char **argv);

} // namespace cock::sleep

#endif // !SLEEP_COMMAND_HPP
