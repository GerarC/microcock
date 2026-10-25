/** @file
 * @brief The letter `ps` prints for the state of a process.
 */
#ifndef PS_STATE_HPP
#define PS_STATE_HPP

#include <cock/abi/pm.hpp>

namespace cock::ps {

/**
 * @brief Gives the letter of a process state.
 *
 * @param state State as `pm` reports it.
 * @return `R` running, `S` sleeping, `T` stopped, `Z` zombie, `X` exited, `D` uninterruptible, `?` for anything else.
 */
char stateLetter(cock::abi::pm::State state);

} // namespace cock::ps

#endif // !PS_STATE_HPP
