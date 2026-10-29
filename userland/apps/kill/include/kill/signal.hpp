/** @file
 * @brief The signals `kill` can send, by name and by number.
 */
#ifndef KILL_SIGNAL_HPP
#define KILL_SIGNAL_HPP

#include <stdint.h>

namespace cock::kill {

/** @brief Character that starts the signal argument. */
constexpr char SIGNAL_PREFIX = '-';

/**
 * @brief Reads a signal as typed after the dash: a name or a number.
 *
 * @param text Text after the `-`, such as `KILL` or `9`.
 * @param signal Receives the signal number of `abi::process`; left unchanged on failure.
 * @return True when the text names `INT`, `KILL`, `TERM`, `CONT` or `STOP`, or is exactly one of their numbers; false
 * for anything else.
 */
bool parseSignal(const char *text, uint32_t &signal);

} // namespace cock::kill

#endif // !KILL_SIGNAL_HPP
