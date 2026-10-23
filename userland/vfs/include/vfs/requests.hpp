/** @file
 * @brief One handler for each kind of request a client sends to the `vfs` server.
 *
 * Every handler answers the client with `FS_REPLY`, success or not; none returns a value.
 * Requests from a process without a session never reach them: `handle` refuses them.
 */
#ifndef VFS_REQUESTS_HPP
#define VFS_REQUESTS_HPP

#include <cock/ipc/message.hpp>
#include <cock/user/fs.hpp>
#include <vfs/clients.hpp>

namespace cock::vfs {

/**
 * @brief Handles one message: `FS_CONNECT`, or a file request from a connected client.
 *
 * Messages from the kernel and message types that are not file requests are ignored. A file
 * request from a process without a session gets `NOT_CONNECTED`.
 *
 * @param message Message received, never trusted.
 */
void handle(const cock::ipc::Message &message);

/**
 * @brief Handles `FS_OPEN`: routes the path and opens the file at its server.
 *
 * @param session Session of the sender.
 * @param message Request: path length in the second argument, open flags in the third.
 * @note Replies `UNSUPPORTED` for unknown flags and `NO_SPACE` when the table is full even
 * after releasing the files of dead clients; the handle is the slot index plus one.
 */
void openFile(Session &session, const cock::ipc::Message &message);

/**
 * @brief Handles `FS_READ`: reads from the server into the window of the client.
 *
 * @param session Session of the sender.
 * @param message Request: handle, offset and length in the second to fourth arguments.
 * @note Replies `BAD_HANDLE` for a handle the sender does not own; the value is the byte count.
 */
void readFile(Session &session, const cock::ipc::Message &message);

/**
 * @brief Handles `FS_WRITE`: writes the window of the client to the server.
 *
 * @param session Session of the sender.
 * @param message Request: handle, offset and length in the second to fourth arguments.
 * @note Replies `BAD_HANDLE` for a handle the sender does not own; the value is the byte count.
 */
void writeFile(Session &session, const cock::ipc::Message &message);

/**
 * @brief Handles `FS_STAT`: size and type of an open file.
 *
 * @param message Request: handle in the second argument.
 * @note Replies `BAD_HANDLE` for a handle the sender does not own.
 */
void statFile(const cock::ipc::Message &message);

/**
 * @brief Handles `FS_CLOSE`: closes the file at its server and frees the slot.
 *
 * @param message Request: handle in the second argument.
 * @note The slot is freed even when the server fails to close the file.
 */
void closeFile(const cock::ipc::Message &message);

/**
 * @brief Handles a request that takes one path and one operation of the server.
 *
 * Used for `FS_MKDIR` and `FS_RMDIR`.
 *
 * @param session Session of the sender.
 * @param message Request: path length in the second argument.
 * @param operation Member of the backend client to call with the path inside the mount.
 */
void pathRequest(Session &session, const cock::ipc::Message &message,
				 bool (cock::user::fs::Client::*operation)(const char *));

/**
 * @brief Handles `FS_UNLINK`: removes a file at the server that owns it.
 *
 * @param session Session of the sender.
 * @param message Request: path length in the second argument.
 */
void unlinkFile(Session &session, const cock::ipc::Message &message);

/**
 * @brief Handles `FS_RENAME`: renames inside one mount.
 *
 * @param session Session of the sender.
 * @param message Request: lengths of the old and the new path in the second and third
 * arguments; the new path starts at `RENAME_NEW_OFFSET` in the window.
 * @note Replies `CROSS_DEVICE` when the two paths belong to different mounts.
 */
void renameEntry(Session &session, const cock::ipc::Message &message);

/**
 * @brief Handles `FS_LINK`: makes a hard link inside one mount.
 *
 * @param session Session of the sender.
 * @param message Request: lengths of the existing and the new path in the second and third
 * arguments; the new path starts at `RENAME_NEW_OFFSET` in the window.
 * @note Replies `CROSS_DEVICE` when the two paths belong to different mounts.
 */
void linkEntry(Session &session, const cock::ipc::Message &message);

/**
 * @brief Handles `FS_READDIR`: one entry of a directory.
 *
 * The listing of `/` shows the entries of the server on `/` first, then one directory for
 * each mount with a single component prefix.
 *
 * @param session Session of the sender.
 * @param message Request: handle and entry index in the second and third arguments.
 * @note Replies `OUT_OF_RANGE` past the last entry; the name is in the window.
 */
void listDirectory(Session &session, const cock::ipc::Message &message);

} // namespace cock::vfs

#endif // !VFS_REQUESTS_HPP
