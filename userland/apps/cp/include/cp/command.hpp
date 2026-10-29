/** @file
 * @brief `cp from to`: copies a file.
 */
#ifndef CP_COMMAND_HPP
#define CP_COMMAND_HPP

namespace cock::cp {

/**
 * @brief Copies the bytes of one file into another, creating it or replacing its content.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name, the source path and the destination path.
 * @return 0 when the whole file was copied; 1 after printing `cp: <path>: <reason>` when a file cannot be opened,
 * read or written; 2 after printing `usage: cp from to` for any other count of arguments.
 */
int run(int argc, char **argv);

} // namespace cock::cp

#endif // !CP_COMMAND_HPP
