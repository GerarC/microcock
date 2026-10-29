/** @file
 * @brief The prompt of the shell: its name and the working directory, in color.
 */
#ifndef SH_PROMPT_HPP
#define SH_PROMPT_HPP

namespace cock::sh {

/**
 * @brief Prints the prompt, `cock:<directory>> `, with the name in bright green and the directory in bright cyan.
 *
 * The colors are escape sequences (`cock/abi/ansi.hpp`) that the console driver draws on the screen and drops from the
 * serial line. When the working directory cannot be read the directory is shown as `?`.
 */
void printPrompt();

/**
 * @brief Prints the line that greets the user at the start of the shell, with the name of the system in color.
 */
void printBanner();

} // namespace cock::sh

#endif // !SH_PROMPT_HPP
