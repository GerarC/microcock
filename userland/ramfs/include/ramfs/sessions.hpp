/** @file
 * @brief The sessions of the clients of the `ramfs` server and the files each one has open.
 */
#ifndef RAMFS_SESSIONS_HPP
#define RAMFS_SESSIONS_HPP

#include <ramfs/archive.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::ramfs {

/** @brief Number of client sessions, each with its own window, kept at once. */
constexpr size_t MAX_CLIENTS = 8;

/** @brief Number of files one client can have open at once. */
constexpr size_t MAX_HANDLES = 16;

/**
 * @brief A connected client, the window it shares with `ramfs` and its open files.
 */
struct Session {
	bool used; /**< True when the slot holds a session. */
	uint32_t pid; /**< Process id of the client. */
	uint32_t windowId; /**< Id of the shared memory object that is the window. */
	uint8_t *window; /**< Where `ramfs` mapped the window. */
	bool open[MAX_HANDLES]; /**< True for each handle slot in use. */
	uint32_t entry[MAX_HANDLES]; /**< Index into `entries` of the file in each slot. */
};

/** @brief The sessions, one per client, up to `MAX_CLIENTS`. */
extern Session sessions[MAX_CLIENTS];

/**
 * @brief Frees the sessions of dead clients.
 *
 * A client is dead when `SYS_PROC_ALIVE` says so.
 */
void releaseDeadSessions();

/**
 * @brief Answers `FS_CONNECT`: opens a session, or reuses the one a client already has.
 *
 * A client that connects again loses the files it had open. When no session is free, the
 * sessions of dead clients are released first.
 *
 * @param pid Process id of the client.
 * @param id Request id to echo in the reply.
 * @note Replies `NO_SPACE` when no session can be opened.
 */
void connect(uint32_t pid, uint32_t id);

/**
 * @brief Finds the entry of an open handle.
 *
 * @param session Session of the client.
 * @param handle Handle as the client sees it, starting at one.
 * @return The entry, or null when the handle is zero, out of range or not open.
 */
const Entry *handleEntry(const Session &session, uint32_t handle);

} // namespace cock::ramfs

#endif // !RAMFS_SESSIONS_HPP
