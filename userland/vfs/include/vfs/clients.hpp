/** @file
 * @brief The sessions of the clients of the `vfs` server, each with a shared window.
 */
#ifndef VFS_CLIENTS_HPP
#define VFS_CLIENTS_HPP

#include <vfs/limits.hpp>
#include <stdint.h>

namespace cock::vfs {

/**
 * @brief A connected client and the window it shares with `vfs` for data and paths.
 */
struct Session {
	bool used; /**< True when the slot holds a session. */
	uint32_t pid; /**< Process id of the client. */
	uint32_t windowId; /**< Id of the shared memory object that is the window. */
	uint8_t *window; /**< Where `vfs` mapped the window. */
};

/** @brief The sessions, one per client, up to `MAX_CLIENTS`. */
extern Session sessions[MAX_CLIENTS];

/**
 * @brief Closes the files of dead clients and frees their sessions.
 *
 * A client is dead when `SYS_PROC_ALIVE` says so.
 */
void releaseDead();

/**
 * @brief Answers `FS_CONNECT`: opens a session, or reuses the one a client already has.
 *
 * A client that connects again loses the files it had open. When no session is free,
 * the sessions of dead clients are released first.
 *
 * @param token Reply token the request carried.
 * @param client Badge of the client, which is its process id.
 * @param id Request id to echo in the reply.
 * @note Replies `NO_SPACE` when no session can be opened.
 */
void connect(uint32_t token, uint32_t client, uint32_t id);

} // namespace cock::vfs

#endif // !VFS_CLIENTS_HPP
