/** @file
 * @brief What an answer of `pm` to a signal means, in words.
 */
#ifndef KILL_OUTCOME_HPP
#define KILL_OUTCOME_HPP

#include <cock/abi/pm.hpp>

namespace cock::kill {

/**
 * @brief Turns the status of a `KILL` request into words.
 *
 * @param status What `pm` answered.
 * @return Text such as `not permitted`, `no such process` or `refused`; `failed` for a status with no text of its own.
 */
const char *describe(cock::abi::pm::Status status);

} // namespace cock::kill

#endif // !KILL_OUTCOME_HPP
