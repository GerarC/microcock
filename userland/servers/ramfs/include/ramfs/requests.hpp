/** @file
 * @brief One handler for each kind of request a client sends to the `ramfs` server.
 *
 * The file system is read only. Every handler answers the client with `FS_REPLY`, success or
 * not; none returns a value. Requests from a process without a session never reach them:
 * `handle` refuses them.
 */
#ifndef RAMFS_REQUESTS_HPP
#define RAMFS_REQUESTS_HPP

#include <cock/ipc/message.hpp>
#include <ramfs/sessions.hpp>

namespace cock::ramfs {

/**
 * @brief Handles one message: `FS_CONNECT`, or a file request from a connected client.
 *
 * Messages from the kernel and message types that are not file requests are ignored. A file
 * request from a process without a session gets `NOT_CONNECTED`. Writing requests get
 * `READ_ONLY`.
 *
 * @param message Message received, never trusted.
 */
void handle(const cock::ipc::Message &message);

/**
 * @brief Handles `FS_OPEN`: finds the path and gives a handle.
 *
 * @param session Session of the sender.
 * @param message Request: path length in the second argument, open flags in the third.
 * @note Replies `UNSUPPORTED` for unknown flags, `READ_ONLY` for anything but read only,
 * `BAD_REQUEST` for a bad path, `NOT_FOUND` for a missing entry and `NO_SPACE` when the
 * client has no free handle; the handle is the slot index plus one.
 */
void openFile(Session &session, const cock::ipc::Message &message);

/**
 * @brief Handles `FS_READ`: copies file data into the window of the client.
 *
 * @param session Session of the sender.
 * @param message Request: handle, offset and length in the second to fourth arguments.
 * @note Replies `BAD_HANDLE` for a bad handle, `UNSUPPORTED` for a directory and
 * `OUT_OF_RANGE` for an offset past the end; the value is the byte count, at most the window.
 */
void readFile(Session &session, const cock::ipc::Message &message);

/**
 * @brief Handles `FS_STAT`: size and type of an open entry.
 *
 * @param session Session of the sender.
 * @param message Request: handle in the second argument.
 * @note Replies `BAD_HANDLE` for a bad handle.
 */
void statFile(Session &session, const cock::ipc::Message &message);

/**
 * @brief Handles `FS_READDIR`: one entry of a directory.
 *
 * @param session Session of the sender.
 * @param message Request: handle and entry index in the second and third arguments.
 * @note Replies `BAD_HANDLE` for a bad handle, `UNSUPPORTED` for a file and `OUT_OF_RANGE`
 * past the last entry; the name is in the window.
 */
void listDirectory(Session &session, const cock::ipc::Message &message);

/**
 * @brief Handles `FS_CLOSE`: frees the handle.
 *
 * @param session Session of the sender.
 * @param message Request: handle in the second argument.
 * @note Replies `BAD_HANDLE` for a bad handle.
 */
void closeFile(Session &session, const cock::ipc::Message &message);

} // namespace cock::ramfs

#endif // !RAMFS_REQUESTS_HPP
