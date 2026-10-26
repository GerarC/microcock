/** @file
 * @brief `touch path...`: creates empty files.
 */
#ifndef TOUCH_COMMAND_HPP
#define TOUCH_COMMAND_HPP

namespace cock::touch {

/**
 * @brief Creates each file that does not exist, empty. A file that exists is left as it is.
 *
 * The file system has no call that changes a time without writing, so unlike the Unix `touch` it never updates the
 * modification time of an existing file.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and one or more paths.
 * @return 0 when every file exists afterwards; 1 after printing `touch: <path>: <reason>` for each one that could not
 * be created (the others are still made); 2 after printing `usage: touch path...` when there is no path.
 */
int run(int argc, char **argv);

} // namespace cock::touch

#endif // !TOUCH_COMMAND_HPP
