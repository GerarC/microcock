/** @file
 * @brief The operations that change the volume: create, write, truncate, remove and rename.
 *
 * Each one clears the clean shutdown bit, does its work and sets the bit again, so a crash in
 * the middle leaves a volume that is mounted read only the next time.
 */
#ifndef FAT_OPERATIONS_HPP
#define FAT_OPERATIONS_HPP

#include <cock/abi/fs.hpp>
#include <fat/volume.hpp>
#include <stdint.h>

namespace cock::fat {

/**
 * @brief Creates an empty regular file.
 *
 * @param path Path of the new file; its parent must exist.
 * @param node Receives the new file.
 * @return `OK`, `EXISTS`, `NOT_FOUND` for a missing parent, `BAD_REQUEST` for a bad name,
 * `NO_SPACE`, or `IO_ERROR`.
 */
cock::abi::fs::Status createFile(const char *path, Node &node);

/**
 * @brief Creates a directory with its `.` and `..` entries.
 *
 * @param path Path of the new directory; its parent must exist.
 * @return `OK`, `EXISTS`, `NOT_FOUND`, `BAD_REQUEST`, `NO_SPACE`, or `IO_ERROR`. A failure
 * frees the cluster taken for the directory.
 */
cock::abi::fs::Status makeDirectory(const char *path);

/**
 * @brief Removes an empty directory.
 *
 * @param path Path of the directory; the root cannot be removed.
 * @return `OK`, `NOT_FOUND` for a missing path or one that is not a directory, `NOT_EMPTY`,
 * or `IO_ERROR`.
 */
cock::abi::fs::Status removeDirectory(const char *path);

/**
 * @brief Renames or moves a file or a directory, replacing a compatible target.
 *
 * A directory cannot move inside itself. A file replaces a file and a directory replaces an
 * empty directory. A change of case only keeps the same clusters.
 *
 * @param from Existing path.
 * @param to New path; its parent must exist.
 * @return `OK`, `NOT_FOUND`, `BAD_REQUEST`, `EXISTS` for a target of another type,
 * `NOT_EMPTY`, `NO_SPACE`, or `IO_ERROR`.
 */
cock::abi::fs::Status renameFile(const char *from, const char *to);

/**
 * @brief Cuts a file to zero length.
 *
 * @param node The file.
 * @return `OK` or `IO_ERROR`.
 */
cock::abi::fs::Status truncateFile(Node &node);

/**
 * @brief Writes bytes to a file.
 *
 * @param handle The open file.
 * @param source The bytes to write.
 * @param offset First byte to write.
 * @param count Number of bytes.
 * @param written Receives the number of bytes stored.
 * @return `OK`, `NO_SPACE` when nothing fitted, or `IO_ERROR`.
 */
cock::abi::fs::Status writeFile(Handle &handle, const uint8_t *source, uint32_t offset,
								uint32_t count, uint32_t &written);

/**
 * @brief Removes a regular file and frees its clusters.
 *
 * @param path Path of the file.
 * @return `OK`, `NOT_FOUND`, `UNSUPPORTED` for a directory, or `IO_ERROR`.
 */
cock::abi::fs::Status unlinkFile(const char *path);

} // namespace cock::fat

#endif // !FAT_OPERATIONS_HPP
