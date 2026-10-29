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

/** @brief Word count of a line with one command and one argument. */
constexpr size_t ONE_ARGUMENT = 2;

/** @brief Character that ends a line of input. */
constexpr char NEWLINE = '\n';

} // namespace cock::sh

#endif // !SH_ARGUMENTS_HPP
