/** @file
 * @brief Paths typed at the prompt: turning them into absolute ones against the working directory.
 */
#ifndef SH_PATHS_HPP
#define SH_PATHS_HPP

#include <cock/abi/fs.hpp>
#include <stddef.h>

namespace cock::sh {

/** @brief Size in bytes of a buffer that holds an absolute path, the terminator included. */
constexpr size_t PATH_BYTES = cock::abi::fs::MAX_PATH_BYTES;

/**
 * @brief Resolves a path of a command against the working directory of the shell.
 *
 * The file servers see absolute paths only, so every command that takes a path calls this first. The directory is the
 * one `cd` last set (`getcwd`).
 *
 * @param command Name of the command, for the message.
 * @param path The word as typed: absolute or relative, with `.` and `..` allowed.
 * @param out Receives the absolute, normalized path.
 * @return True on success. False after printing `sh: <command>: <path>: path too long` when the result does not fit.
 */
bool resolvePath(const char *command, const char *path, char (&out)[PATH_BYTES]);

} // namespace cock::sh

#endif // !SH_PATHS_HPP
