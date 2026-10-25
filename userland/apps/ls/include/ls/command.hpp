/** @file
 * @brief `ls [path]`: lists a directory.
 */
#ifndef LS_COMMAND_HPP
#define LS_COMMAND_HPP

namespace cock::ls {

/**
 * @brief Lists the directory the first argument names, the working directory without one.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and at most one path.
 * @return 0 on success; 1 after a failure message (`ls: <path>: not found`, ...); 2 after printing `usage: ls [path]`
 * for more than one argument.
 */
int run(int argc, char **argv);

} // namespace cock::ls

#endif // !LS_COMMAND_HPP
