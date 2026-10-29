/** @file
 * @brief Reading a directory through the file server.
 */
#ifndef LS_DIRECTORY_HPP
#define LS_DIRECTORY_HPP

namespace cock::ls {

/**
 * @brief Prints the entries of a directory, one per line; directories end in a slash.
 *
 * For a file it prints the path as given. The first call connects to the file server.
 *
 * @param program Name of the program, for the messages.
 * @param path Path as typed; it is resolved against the working directory.
 * @return 0 on success; 1 after printing `<program>: <path>: <reason>` on failure.
 */
int list(const char *program, const char *path);

} // namespace cock::ls

#endif // !LS_DIRECTORY_HPP
