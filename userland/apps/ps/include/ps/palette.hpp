/** @file
 * @brief The colors `ps` prints with, as the escape sequences `cock/abi/ansi.hpp` reads.
 *
 * The screen driver draws them and the serial line drops them, so the text of the table is the same on both.
 */
#ifndef PS_PALETTE_HPP
#define PS_PALETTE_HPP

namespace cock::ps {

/** Ends every color: the text that follows has the default colors. */
constexpr char RESET[] = "\x1B[0m";
/** The titles of the columns. */
constexpr char HEADER[] = "\x1B[96m";
/** The process id and the parent id. */
constexpr char NUMBER[] = "\x1B[93m";
/** The name of a live process. */
constexpr char NAME[] = "\x1B[97m";
/** A process that has exited, whatever its name, and a missing value. */
constexpr char MUTED[] = "\x1B[90m";
/** State `R`. */
constexpr char RUNNING[] = "\x1B[92m";
/** State `S`. */
constexpr char SLEEPING[] = "\x1B[96m";
/** State `T`. */
constexpr char STOPPED[] = "\x1B[93m";
/** State `Z`. */
constexpr char ZOMBIE[] = "\x1B[91m";
/** State `D` and any state `ps` does not know. */
constexpr char OTHER[] = "\x1B[95m";

} // namespace cock::ps

#endif // !PS_PALETTE_HPP
