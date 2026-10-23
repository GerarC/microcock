/** @file
 * @brief The backend the shared file server template needs, and the server built on it.
 */
#ifndef EXT_SERVER_HPP
#define EXT_SERVER_HPP

#include <cock/abi/ext2.hpp>
#include <cock/abi/fs.hpp>
#include <cock/user/file_server.hpp>
#include <ext/directory.hpp>
#include <ext/file.hpp>
#include <ext/lookup.hpp>
#include <ext/operations.hpp>
#include <ext/volume.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::ext {

/**
 * @brief Adapts the functions of this program to what `cock::user::FileServer` calls.
 *
 * Every member forwards to a function of the other modules; the compiler inlines them.
 */
struct Backend {
	/** @brief A file, directory or symbolic link. */
	using Node = cock::ext::Node;

	/** @brief An open file. */
	using Handle = cock::ext::Handle;

	/** @brief Largest file size the server accepts. */
	static constexpr uint32_t MAX_FILE_BYTES = cock::abi::ext2::MAX_FILE_BYTES;

	/** @brief True when the volume accepts writes. */
	static bool writable() { return cock::ext::writable(); }

	/** @brief True when two nodes are the same file. */
	static bool sameFile(const Node &first, const Node &second) {
		return first.inode == second.inode;
	}

	/** @brief Resolves a path to a node, following links; see `cock::ext::resolve`. */
	static bool resolve(const char *path, Node &node) { return cock::ext::resolve(path, node); }

	/** @brief Resolves a path without following a link in its last component. */
	static bool resolveLeaf(const char *path, Node &node) {
		return cock::ext::resolveLeaf(path, node);
	}

	/** @brief Reads bytes of a file; see `readBytes`. */
	static bool read(Handle &handle, uint8_t *destination, uint32_t offset, uint32_t count) {
		return readBytes(handle.node, destination, offset, count);
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

	/** @brief Cuts a file to zero length; see `truncateFile`. */
	static cock::abi::fs::Status truncate(Node &node) { return truncateFile(node); }

	/** @brief Removes a file; see `unlinkFile`. */
	static cock::abi::fs::Status unlink(const char *path) { return unlinkFile(path); }

	/** @brief Makes a directory; see `cock::ext::makeDirectory`. */
	static cock::abi::fs::Status makeDirectory(const char *path) {
		return cock::ext::makeDirectory(path);
	}

	/** @brief Removes a directory; see `cock::ext::removeDirectory`. */
	static cock::abi::fs::Status removeDirectory(const char *path) {
		return cock::ext::removeDirectory(path);
	}

	/** @brief Renames an entry; see `renameFile`. */
	static cock::abi::fs::Status rename(const char *from, const char *to) {
		return renameFile(from, to);
	}

	/** @brief Adds a hard link; see `linkFile`. */
	static cock::abi::fs::Status link(const char *existing, const char *name) {
		return linkFile(existing, name);
	}

	/** @brief Writes bytes to a file; see `writeFile`. */
	static cock::abi::fs::Status write(Handle &handle, const uint8_t *source, uint32_t offset,
									   uint32_t count, uint32_t &written) {
		return writeFile(handle, source, offset, count, written);
	}
};

/** @brief The file server over the ext2 backend. */
using Server = cock::user::FileServer<Backend>;

} // namespace cock::ext

#endif // !EXT_SERVER_HPP
