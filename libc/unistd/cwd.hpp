/**
 * @file cwd.hpp
 * @brief The working directory of a process, kept by the C library.
 *
 * The directory is a string in the memory of the process, so `fork` copies it with the rest of the address space and
 * a program that replaces its image with `exec` writes it into the argument block (`abi::exec::ArgsBlock::cwd`), as
 * the descriptor table does. A process is born in `/`. The file calls (`open`, `unlink`, `mkdir`, `rmdir`, `rename`,
 * `link`, `chdir`) turn the path they receive into an absolute one with `absolutePath` before they talk to `vfs`, so
 * the servers see absolute paths only.
 */
#ifndef LIBC_UNISTD_CWD_HPP
#define LIBC_UNISTD_CWD_HPP

#include <cock/abi/exec.hpp>
#include <cock/abi/fs.hpp>
#include <stddef.h>

namespace cock::libc {

/** Size of a buffer that holds an absolute path, terminator included: what the file servers accept. */
constexpr size_t PATH_BUFFER_BYTES = cock::abi::fs::MAX_PATH_BYTES;

/**
 * @brief Resolves a path against the working directory.
 *
 * @param path The path as the caller gave it, not null; absolute or relative, with `.` and `..` allowed.
 * @param out Receives the absolute, normalized path.
 * @return True on success. False with `errno` set to `ENOENT` for an empty path or `ENAMETOOLONG` when the result does not
 * fit in `PATH_BUFFER_BYTES`.
 */
bool absolutePath(const char *path, char (&out)[PATH_BUFFER_BYTES]);

} // namespace cock::libc

extern "C" {

/**
 * @brief Writes the working directory into the argument block of an `exec`.
 *
 * Called by the process manager client when a program replaces its image, next to `__descriptors_export`.
 *
 * @param block The block `cock::exec::build` filled; its `cwd` field is set.
 */
void __cwd_export(cock::abi::exec::ArgsBlock *block);

/**
 * @brief Takes the working directory an `exec` handed on.
 *
 * Called by the start code of a program started by `exec`, before the constructors run. An empty directory in the block
 * leaves the process in `/`.
 *
 * @param block A block that passed `cock::exec::valid`.
 */
void __cwd_import(const cock::abi::exec::ArgsBlock *block);

}

#endif // !LIBC_UNISTD_CWD_HPP
