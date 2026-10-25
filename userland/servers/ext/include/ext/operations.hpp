/** @file
 * @brief The operations that change the volume: create, write, truncate, remove, rename, link.
 *
 * Each one clears the `VALID` flag of the superblock, does its work and sets the flag again,
 * so a crash in the middle leaves a volume that `e2fsck` checks and the server will not write.
 */
#ifndef EXT_OPERATIONS_HPP
#define EXT_OPERATIONS_HPP

#include <cock/abi/fs.hpp>
#include <ext/volume.hpp>
#include <stdint.h>

namespace cock::ext {

/**
 * @brief Creates an empty regular file.
 *
 * @param path Path of the new file; its parent must exist.
 * @param node Receives the new file.
 * @return `OK`, `EXISTS`, `NOT_FOUND` for a missing parent, `BAD_REQUEST` for `.` or `..`,
 * `NO_SPACE`, or `IO_ERROR`.
 */
cock::abi::fs::Status createFile(const char *path, Node &node);

/**
 * @brief Cuts a file to zero length.
 *
 * @param node The file.
 * @return `OK`, `UNSUPPORTED` for a file of 2 GiB or more, or `IO_ERROR`.
 */
cock::abi::fs::Status truncateFile(Node &node);

/**
 * @brief Removes a name of a regular file and frees the file when it was the last one.
 *
 * @param path Path of the name.
 * @return `OK`, `NOT_FOUND`, `UNSUPPORTED` for anything but a regular file below 2 GiB, or
 * `IO_ERROR`.
 */
cock::abi::fs::Status unlinkFile(const char *path);

/**
 * @brief Creates a directory with its `.` and `..` records.
 *
 * @param path Path of the new directory; its parent must exist.
 * @return `OK`, `EXISTS`, `NOT_FOUND`, `BAD_REQUEST` for `.` or `..`, `NO_SPACE`, or
 * `IO_ERROR`. A failure gives back the inode and the block taken.
 */
cock::abi::fs::Status makeDirectory(const char *path);

/**
 * @brief Removes an empty directory.
 *
 * @param path Path of the directory; the root cannot be removed.
 * @return `OK`, `NOT_FOUND`, `UNSUPPORTED` for the root or a non-directory, `NOT_EMPTY`, or
 * `IO_ERROR`.
 */
cock::abi::fs::Status removeDirectory(const char *path);

/**
 * @brief Renames or moves a file or a directory, replacing a compatible target.
 *
 * A directory cannot move inside itself. A file replaces a file and a directory replaces an
 * empty directory.
 *
 * @param from Existing path.
 * @param to New path; its parent must exist.
 * @return `OK`, `NOT_FOUND`, `BAD_REQUEST`, `EXISTS` for a target of another type,
 * `NOT_EMPTY`, `NO_SPACE`, or `IO_ERROR`.
 */
cock::abi::fs::Status renameFile(const char *from, const char *to);

/**
 * @brief Adds a hard link to a regular file.
 *
 * @param existing Path of the file.
 * @param name Path of the new name; its parent must exist.
 * @return `OK`, `NOT_FOUND`, `EXISTS`, `BAD_REQUEST` for `.` or `..`, `UNSUPPORTED` for a
 * directory or a link, `NO_SPACE` when the file has the largest link count or the directory
 * cannot grow, or `IO_ERROR`.
 */
cock::abi::fs::Status linkFile(const char *existing, const char *name);

/**
 * @brief Writes bytes to a file.
 *
 * @param handle The open file.
 * @param source The bytes to write.
 * @param offset First byte to write.
 * @param count Number of bytes.
 * @param written Receives the number of bytes stored.
 * @return `OK`, `NO_SPACE` when nothing fitted, `UNSUPPORTED` for a file of 2 GiB or more, or
 * `IO_ERROR`.
 */
cock::abi::fs::Status writeFile(Handle &handle, const uint8_t *source, uint32_t offset,
								uint32_t count, uint32_t &written);

} // namespace cock::ext

#endif // !EXT_OPERATIONS_HPP
