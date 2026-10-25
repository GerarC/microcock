/** @file
 * @brief The backend the shared file server template needs, and the server built on it.
 */
#ifndef FAT_SERVER_HPP
#define FAT_SERVER_HPP

#include <cock/abi/fs.hpp>
#include <cock/user/file_server.hpp>
#include <fat/directory.hpp>
#include <fat/file.hpp>
#include <fat/lookup.hpp>
#include <fat/operations.hpp>
#include <fat/volume.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::fat {

/** @brief Largest size of a file: FAT stores sizes in 32 bits. */
constexpr uint32_t MAX_FILE_SIZE = 0xFFFFFFFF;

/**
 * @brief Adapts the functions of this program to what `cock::user::FileServer` calls.
 *
 * Every member forwards to a function of the other modules; the compiler inlines them.
 */
struct Backend {
	/** @brief A file or directory. */
	using Node = cock::fat::Node;

	/** @brief An open file. */
	using Handle = cock::fat::Handle;

	/** @brief Largest file size the server accepts. */
	static constexpr uint32_t MAX_FILE_BYTES = MAX_FILE_SIZE;

	/** @brief True when the volume accepts writes. */
	static bool writable() { return cock::fat::writable(); }

	/** @brief True when two nodes are the same file. */
	static bool sameFile(const Node &first, const Node &second) {
		return sameSpot(first.entry, second.entry);
	}

	/** @brief Resolves a path to a node; see `cock::fat::resolve`. */
	static bool resolve(const char *path, Node &node) { return cock::fat::resolve(path, node); }

	/** @brief Resolves the last component of a path; the same as `resolve` on FAT. */
	static bool resolveLeaf(const char *path, Node &node) { return cock::fat::resolve(path, node); }

	/** @brief Reads bytes of a file; see `readBytes`. */
	static bool read(Handle &handle, uint8_t *destination, uint32_t offset, uint32_t count) {
		return readBytes(handle, destination, offset, count);
	}

	/** @brief Gives the n-th entry of a directory; see `listEntry`. */
	static bool list(const Node &directory, uint32_t wanted, char *name, size_t capacity,
					 cock::abi::fs::FileType &type) {
		return listEntry(directory, wanted, name, capacity, type);
	}

	/** @brief Creates a file; see `createFile`. */
	static cock::abi::fs::Status create(const char *path, Node &node) {
		return createFile(path, node);
	}

	/** @brief Makes a directory; see `cock::fat::makeDirectory`. */
	static cock::abi::fs::Status makeDirectory(const char *path) {
		return cock::fat::makeDirectory(path);
	}

	/** @brief Removes a directory; see `cock::fat::removeDirectory`. */
	static cock::abi::fs::Status removeDirectory(const char *path) {
		return cock::fat::removeDirectory(path);
	}

	/** @brief Renames an entry; see `renameFile`. */
	static cock::abi::fs::Status rename(const char *from, const char *to) {
		return renameFile(from, to);
	}

	/** @brief Cuts a file to zero length; see `truncateFile`. */
	static cock::abi::fs::Status truncate(Node &node) { return truncateFile(node); }

	/** @brief Writes bytes to a file; see `writeFile`. */
	static cock::abi::fs::Status write(Handle &handle, const uint8_t *source, uint32_t offset,
									   uint32_t count, uint32_t &written) {
		return writeFile(handle, source, offset, count, written);
	}

	/** @brief Removes a file; see `unlinkFile`. */
	static cock::abi::fs::Status unlink(const char *path) { return unlinkFile(path); }

	/** @brief Hard links do not exist on FAT: always `UNSUPPORTED`. */
	static cock::abi::fs::Status link(const char *, const char *) {
		return cock::abi::fs::Status::UNSUPPORTED;
	}
};

/** @brief The file server over the FAT backend. */
using Server = cock::user::FileServer<Backend>;

} // namespace cock::fat

#endif // !FAT_SERVER_HPP
