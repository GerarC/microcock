/** @file
 * @brief The initial RAM disk as a tar archive in shared memory, and the index of its entries.
 */
#ifndef RAMFS_ARCHIVE_HPP
#define RAMFS_ARCHIVE_HPP

#include <cock/abi/fs.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::ramfs {

/** @brief Size in bytes of the name field of a tar header, and so the longest name. */
constexpr size_t TAR_NAME_BYTES = 100;

/** @brief Capacity of the index: the root and the entries of the archive. */
constexpr size_t MAX_ENTRIES = 128;

/** @brief Index of the first entry that comes from the archive; entry zero is the root. */
constexpr size_t FIRST_FILE_ENTRY = 1;

/**
 * @brief A file or a directory of the archive.
 */
struct Entry {
	char name[TAR_NAME_BYTES + 1]; /**< Path without a leading or trailing separator. */
	uint32_t offset; /**< Byte offset of the data inside the archive. */
	uint32_t size; /**< Size of the data in bytes; zero for a directory. */
	cock::abi::fs::FileType type; /**< Regular file or directory. */
};

/** @brief The mapped archive, read only. Null until `loadArchive` succeeds. */
extern const uint8_t *archive;

/** @brief The index. Entry zero is the root directory; only the first `entryCount` are valid. */
extern Entry entries[MAX_ENTRIES];

/** @brief Number of valid entries in `entries`, root included. */
extern size_t entryCount;

/**
 * @brief Opens the `initrd` data module, maps it read only and builds the index.
 *
 * Needs the capability to open boot modules, so it must run after `CAPABILITY_GRANTED`. The
 * index stops at the first block that is not a valid tar header, at a size above the limit of
 * a shared object, or when it is full.
 *
 * @return True when the archive is mapped, false when the module cannot be opened or mapped.
 */
bool loadArchive();

/**
 * @brief Finds an entry by its path.
 *
 * A leading separator and trailing separators are ignored; the empty path is the root.
 *
 * @param path Path, terminated by zero.
 * @return The entry, or null when there is none or the path is longer than a name can be.
 */
const Entry *findEntry(const char *path);

/**
 * @brief Tells whether an entry is a direct child of a directory.
 *
 * @param directory The directory.
 * @param candidate The entry to test.
 * @param name Receives the name of the candidate inside the directory when the result is
 * true.
 * @return True for a direct child, false for the directory itself, a deeper descendant or an
 * entry elsewhere.
 */
bool isChild(const Entry &directory, const Entry &candidate, const char *&name);

} // namespace cock::ramfs

#endif // !RAMFS_ARCHIVE_HPP
