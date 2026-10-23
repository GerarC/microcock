/** @file
 * @brief Reading a line of input from the console.
 */
#ifndef SH_INPUT_HPP
#define SH_INPUT_HPP

namespace cock::sh {

/**
 * @brief Reads one line from descriptor 0, which is the console server.
 *
 * Waits until a newline arrives. Characters beyond what a line holds are dropped. When a read
 * returns nothing or fails it sleeps a short time and tries again.
 *
 * @param line Buffer that receives the line, terminated by zero and without the newline; it
 * must hold `cock::shell::MAX_LINE_BYTES` bytes.
 */
void readLine(char *line);

} // namespace cock::sh

#endif // !SH_INPUT_HPP
