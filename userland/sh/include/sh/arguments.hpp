/** @file
 * @brief Positions of the words of a command line and sizes shared by the shell modules.
 */
#ifndef SH_ARGUMENTS_HPP
#define SH_ARGUMENTS_HPP

#include <stddef.h>

namespace cock::sh {

/** @brief Index of the word that names the command. */
constexpr size_t FIRST_WORD = 0;

/** @brief Index of the first argument. */
constexpr size_t SECOND_WORD = 1;

/** @brief Index of the second argument. */
constexpr size_t THIRD_WORD = 2;

/** @brief Word count of a line with one command and one argument. */
constexpr size_t ONE_ARGUMENT = 2;

/** @brief Word count of a line with one command and two arguments. */
constexpr size_t TWO_ARGUMENTS = 3;

/** @brief Size in bytes of the buffer that holds the text of `echo` and `write`. */
constexpr size_t TEXT_BYTES = 256;

/** @brief Character that ends a line of input and of the files `write` makes. */
constexpr char NEWLINE = '\n';

} // namespace cock::sh

#endif // !SH_ARGUMENTS_HPP
