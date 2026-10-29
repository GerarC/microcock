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
	uint32_t handle; /**< The number the client sees: from 1, and unique among the files of that client only. */
	size_t mount; /**< Index into `mounts` of the server that owns the file. */
	uint32_t backend; /**< Handle at that server. */
	bool root; /**< True for the root directory, whose listing also shows the mounts. */
	bool countKnown; /**< True once `backendEntries` has been counted. */
	uint32_t backendEntries; /**< Entries the server reports for the root directory. */
};

/** @brief The slots of open files. The handle a client sees is in `Open::handle`, not the slot index. */
extern Open opens[MAX_OPEN];

/**
 * @brief Finds an open file by the handle a client holds.
 *
 * @param pid Process id of the client.
 * @param number Handle as the client sees it.
 * @return The slot, or null when the handle is zero, or the client has no file with that number.
 */
Open *findOpen(uint32_t pid, uint32_t number);

/**
 * @brief Finds a free slot in the table.
 *
 * @return A free slot, or null when the table is full.
 */
Open *freeOpen();

/**
 * @brief Gives the lowest handle number that a client does not use for another file.
 *
 * @param pid Process id of the client.
 * @return A number from 1.
 */
uint32_t newHandle(uint32_t pid);

/**
 * @brief Frees the slot of an open file and closes the file at its server unless another slot still names it.
 *
 * A copy made by `fork` holds a slot of its own on the same file at the server, so the server is told only when
 * the last of them closes.
 *
 * @param open Slot in use.
 * @return false when the server refused to close the file; the slot is free in every case.
 */
bool releaseOpen(Open &open);

/**
 * @brief Closes an open file at its server and frees the slot, ignoring what the server answers.
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

/**
 * @brief Gives a copy made by `fork` a slot for each file its parent has open, under the same numbers.
 *
 * The slots name the same file at the server, so they share it until each of them closes. The copy gets as many
 * as fit in the table; a call of the copy on another handle is answered `BAD_HANDLE`.
 *
 * @param parent Process id of the process that was copied.
 * @param child Process id of the copy.
 */
void copyOpens(uint32_t parent, uint32_t child);

} // namespace cock::vfs

#endif // !VFS_OPENS_HPP
