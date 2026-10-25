/** @file
 * @brief `mv from to`: renames or moves.
 */
#ifndef MV_COMMAND_HPP
#define MV_COMMAND_HPP

namespace cock::mv {

/**
 * @brief Renames a file or directory; it may change directory but not volume.
 *
 * @param argc Count of `argv`, the program name included.
 * @param argv Name and two paths.
 * @return 0 on success; 1 after printing `mv: <from>: <reason>` on failure (`different volumes` across mounts); 2 after
 * printing `usage: mv from to` for any other count of arguments.
 */
int run(int argc, char **argv);

} // namespace cock::mv

#endif // !MV_COMMAND_HPP
