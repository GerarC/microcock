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

/**
 * @brief Gives the color `ps` prints the letter of a state with.
 *
 * @param state State as `pm` reports it.
 * @return An escape sequence of `ps/palette.hpp`: green for `R`, cyan for `S`, yellow for `T`, red for `Z`, gray for `X`
 * and magenta for `D` and for anything else.
 */
const char *stateColor(cock::abi::pm::State state);

} // namespace cock::ps

#endif // !PS_STATE_HPP
