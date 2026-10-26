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
	bool inherited; /**< True for the session of a copy made by `fork` until the copy connects: it holds the files of its parent. */
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
 * @brief Tells whether a refused call is worth repeating once.
 *
 * The files of a client that ended stay open at the server until `releaseDead` runs, and an open file is exclusive,
 * so the next program to use a file that a short lived program wrote (`echo text > file`, then `cat file`) is answered
 * `BUSY` by the file server. When the last call on the mount was refused with `BUSY`, this releases the files of dead
 * clients, which may be what it found busy.
 *
 * @param index Index into `mounts` of the server that refused the call.
 * @return True when the status was `BUSY` and the dead clients were released: the caller repeats the call once. False
 * for any other status.
 */
bool releasedAfterBusy(size_t index);

/**
 * @brief Answers `FS_CONNECT`: opens a session, or reuses the one a client already has.
 *
 * A client that connects again loses the files it had open, except one that asks to keep them (`keepFiles`) and a copy made by `fork` that connects for the
 * first time: it already has a session with the files of its parent (`inheritSession`), and this call only gives it
 * the window of that session. When no session is free, the sessions of dead clients are released first.
 *
 * @param token Reply token the request carried.
 * @param client Badge of the client, which is its process id.
 * @param keepFiles True when the request carried `CONNECT_KEEP_FILES`: a program that replaced its image with `exec`
 * connects again but still holds the files it opened, which stay open.
 * @param id Request id to echo in the reply.
 * @note The window is shared with the client again every time, because a process that replaced its image with `exec`
 * lost every shared object it held. Replies `NO_SPACE` when no session can be opened or the window cannot be shared.
 */
void connect(uint32_t token, uint32_t client, uint32_t id, bool keepFiles);

/**
 * @brief Gives a copy made by `fork` a session of its own with the files its parent has open.
 *
 * The copy gets a window of its own, so it and its parent never write the same one, and a slot for each open file
 * of the parent under the same handle number, so the handles the copy inherited work. Nothing happens when the
 * parent has no session (it has no files) or the copy already has one. When no session is free the sessions of dead
 * clients are released first; if there is still none the copy is left without, and its first request is answered
 * `NOT_CONNECTED`.
 *
 * @param parent Process id of the process that was copied.
 * @param child Process id of the copy.
 */
void inheritSession(uint32_t parent, uint32_t child);

} // namespace cock::vfs

#endif // !VFS_CLIENTS_HPP
