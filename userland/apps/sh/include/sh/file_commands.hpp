/** @file
 * @brief The built-in commands that use the file system, through the `vfs` client.
 *
 * Every command takes the words of the line, where the first one is the name of the command.
 * The first one that needs a file connects to the file server. A failure prints
 * `sh: <command>: <path>: <reason>` with the reason taken from the status of the server;
 * a wrong count of arguments prints the usage. None returns a status. A path that does not start with `/` is relative
 * to the working directory (`cd`, `pwd`); `sh/paths.hpp` resolves it before the server sees it.
 */
#ifndef SH_FILE_COMMANDS_HPP
#define SH_FILE_COMMANDS_HPP

#include <cock/shell/words.hpp>

namespace cock::sh {

/**
 * @brief `ls [path]`: lists a directory, the working directory by default.
 *
 * Directories end in a slash. For a file it prints the path.
 *
 * @param words Words of the line.
 */
void listCommand(const cock::shell::Words &words);

/**
 * @brief `cat path`: prints a file, byte for byte.
 *
 * @param words Words of the line.
 * @note Prints `sh: cat: <path>: is a directory` for a directory.
 */
void catCommand(const cock::shell::Words &words);

/**
 * @brief `write path text...`: creates or replaces a file with the text and a newline.
 *
 * @param words Words of the line.
 * @note Prints `sh: write: text too long` when the text does not fit.
 */
void writeCommand(const cock::shell::Words &words);

/**
 * @brief `mkdir path`: makes a directory.
 *
 * @param words Words of the line.
 */
void makeDirectoryCommand(const cock::shell::Words &words);

/**
 * @brief `rmdir path`: removes an empty directory.
 *
 * @param words Words of the line.
 */
void removeDirectoryCommand(const cock::shell::Words &words);

/**
 * @brief `rm path`: deletes a file.
 *
 * @param words Words of the line.
 */
void removeCommand(const cock::shell::Words &words);

/**
 * @brief `mv from to`: renames or moves inside one volume.
 *
 * @param words Words of the line.
 */
void moveCommand(const cock::shell::Words &words);

} // namespace cock::sh

#endif // !SH_FILE_COMMANDS_HPP
