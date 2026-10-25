/** @file
 * @brief `write path text...`: replaces a file with a line of text. Temporary: it goes away with the redirection of W8.
 */
#ifndef WRITE_COMMAND_HPP
#define WRITE_COMMAND_HPP

namespace cock::write {

/**
 * @brief Creates the file or empties it, and writes the arguments after the path joined by spaces and a newline.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name, a path and at least one word of text.
 * @return 0 on success; 1 after printing `write: <path>: <reason>` on failure, or `write: text too long` when the line
 * does not fit in 512 bytes; 2 after printing
 * `usage: write path text...` when the path or the text is missing.
 */
int run(int argc, char **argv);

} // namespace cock::write

#endif // !WRITE_COMMAND_HPP
