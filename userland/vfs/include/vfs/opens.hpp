/** @file
 * @brief The table of open files, shared by all clients of the `vfs` server.
 */
#ifndef VFS_OPENS_HPP
#define VFS_OPENS_HPP

#include <vfs/limits.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::vfs {

/**
 * @brief An open file of a client and the handle that the backend server gave for it.
 */
struct Open {
	bool used; /**< True when the slot holds an open file. */
	uint32_t owner; /**< Process id of the client that opened it. */
	size_t mount; /**< Index into `mounts` of the server that owns the file. */
	uint32_t backend; /**< Handle at that server. */
	bool root; /**< True for the root directory, whose listing also shows the mounts. */
	bool countKnown; /**< True once `backendEntries` has been counted. */
	uint32_t backendEntries; /**< Entries the server reports for the root directory. */
};

/** @brief The slots of open files. The handle a client sees is the slot index plus one. */
extern Open opens[MAX_OPEN];

/**
 * @brief Finds an open file by the handle a client holds.
 *
 * @param pid Process id of the client.
 * @param number Handle as the client sees it.
 * @return The slot, or null when the handle is zero, out of range, free or owned by another
 * process.
 */
Open *findOpen(uint32_t pid, uint32_t number);

/**
 * @brief Finds a free slot in the table.
 *
 * @return A free slot, or null when the table is full.
 */
Open *freeOpen();

/**
 * @brief Closes an open file at its server and frees the slot.
 *
 * @param open Slot in use.
 */
void closeOpen(Open &open);

/**
 * @brief Closes every file a client holds.
 *
 * @param pid Process id of the client.
 */
void closeOwnedBy(uint32_t pid);

} // namespace cock::vfs

#endif // !VFS_OPENS_HPP
