/** @file
 * @brief The sessions of the clients of the block server, each with a shared window.
 */
#ifndef ATA_SESSIONS_HPP
#define ATA_SESSIONS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::ata {

/** @brief Number of client sessions, each with its own window, kept at once. */
constexpr size_t MAX_CLIENTS = 8;

/**
 * @brief A connected client and the window it shares with the server for sector data.
 */
struct Session {
	bool used; /**< True when the slot holds a session. */
	uint32_t pid; /**< Process id of the client. */
	uint32_t windowId; /**< Id of the shared memory object that is the window. */
	uint8_t *window; /**< Where the server mapped the window. */
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
 * @brief Answers `BLK_CONNECT`: opens a session, or reuses the one a client already has.
 *
 * The reply carries the window id and the number of devices. When no session is free, the
 * sessions of dead clients are released first.
 *
 * @param token Reply token of the request.
 * @param client Badge of the message, which `names` makes the process id of the client: the
 * session is kept under it and the window is shared with that process, because shared memory is
 * still shared by process id.
 * @param id Request id to echo in the reply.
 * @note Replies `NO_SPACE` when no session can be opened.
 */
void connect(uint32_t token, uint32_t client, uint32_t id);

} // namespace cock::ata

#endif // !ATA_SESSIONS_HPP
