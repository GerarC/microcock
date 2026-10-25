/**
 * @file names.hpp
 * @brief Client of the name server: asks for the right to talk to a server and learns its process id.
 *
 * This replaces the search of a process by name that the kernel used to offer. The call goes to the name
 * server (`abi::service::NAMES_PID`), which checks the `ipc=` policy of the manifest and, when it allows the
 * exchange, gives the calling process an `IPC` capability for the label of the server. From then on the
 * caller may send, call and share memory with that process, and the server may answer.
 *
 * The call is synchronous and uses only registers, so it never touches the inbox of the caller and is safe
 * in the middle of a program that serves requests. A name travels in parts of 12 bytes, one call each, so a
 * path such as `/run/console` takes one or two calls. Any thread of a process may connect: the name server
 * asks the kernel which process the calling thread belongs to.
 */
#ifndef COCK_USER_NAMES_HPP
#define COCK_USER_NAMES_HPP

#include <cock/abi/capability.hpp>
#include <cock/abi/names.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::user::names {

/**
 * Longest wait, in milliseconds, for the answer of the name server. `init` registers a program after it
 * starts running, so the name server keeps the call of an unregistered program until `init` registers it
 * (`names/parked.hpp`); this limit only ends the wait of a program that `init` never registers.
 */
constexpr uint32_t CONNECT_WAIT_MS = 2000;

/** Answer of the name server to a connect request. */
struct Connection {
	/** Result code; the other fields are valid only for `OK`. */
	abi::names::Status status;
	/** Process id of the server. */
	uint32_t pid;
	/** Service label of the server, which covers every process that runs the same program. */
	uint32_t label;
	/** Handle the caller holds on the endpoint of the server (`ep_call`, `ep_send`), whose badge is the process id of the caller; `abi::NO_CAPABILITY_NODE` when the server has no endpoint. The same handle comes back on every connect, and it keeps working when the server restarts. */
	uint32_t handle;
};

/**
 * @brief Sends every part of a name once and returns the answer to the last.
 * @param name The name.
 * @param length Its length, from 1 to `abi::names::MAX_NAME_BYTES`.
 * @param connection Receives the status and, for `OK`, the process id and label.
 * @return false when a call could not be made (the name server is gone); true otherwise.
 */
inline bool sendParts(const char *name, size_t length, Connection &connection) {
	size_t parts = abi::names::partCount(length);
	for (size_t part = 0; part < parts; part++) {
		uint32_t words[abi::names::CALL_WORDS] = {
			abi::names::pack(abi::names::Op::CONNECT, abi::service::NO_SERVICE, part,
							 part + 1 == parts)};
		abi::names::packPart(name, length, part, words);

		CallResult result = ipc_call(abi::service::NAMES_PID, words, CONNECT_WAIT_MS);
		if (result.status == CallStatus::TIMED_OUT) {
			connection.status = abi::names::Status::UNKNOWN_CLIENT;
			return true;
		}
		if (!result.ok) return false;

		connection.status = static_cast<abi::names::Status>(result.words[abi::names::HEADER_WORD]);
		if (connection.status == abi::names::Status::MORE) continue;
		if (connection.status == abi::names::Status::OK) {
			connection.pid = result.words[abi::names::PID_WORD];
			connection.label = result.words[abi::names::LABEL_WORD];
			connection.handle = result.words[abi::names::HANDLE_WORD];
		}
		return true;
	}
	return false;
}

/**
 * @brief Asks the name server for the right to talk to a server.
 *
 * Blocks until the name server answers. Right after a program starts, `init` may not have registered it yet;
 * the name server then holds the call until it does, for at most `CONNECT_WAIT_MS`, and answers `UNKNOWN_CLIENT` if the time runs out.
 * @param name NUL terminated name of the server, 1 to `abi::names::MAX_NAME_BYTES` bytes. A name that starts
 *        with `/` is a path; any other is short for `/run/<name>`.
 * @return `OK` with the process id, the label and the handle; `NOT_FOUND` when no running program has the name;
 *         `DENIED` when the manifest does not let the caller talk to it; `UNKNOWN_CLIENT` when the caller is not
 *         a process `init` registered; `NO_ROOM` when the capability could not be given; `BAD_REQUEST` for an
 *         empty or too long name or when the name server could not be called. In every other case than `OK` the
 *         process id is `NO_PROCESS`.
 */
inline Connection connect(const char *name) {
	Connection connection = {abi::names::Status::BAD_REQUEST, NO_PROCESS, abi::service::NO_SERVICE,
							 abi::NO_CAPABILITY_NODE};

	size_t length = 0;
	while (name[length] != '\0' && length <= abi::names::MAX_NAME_BYTES)
		length++;
	if (length == 0 || length > abi::names::MAX_NAME_BYTES) return connection;

	if (!sendParts(name, length, connection)) connection.status = abi::names::Status::BAD_REQUEST;
	return connection;
}

/**
 * @brief Same as `connect`, for callers that only need the process id.
 * @param name NUL terminated name of the server.
 * @return The process id of the server, or `NO_PROCESS` in every case where `connect` does not return `OK`.
 */
inline uint32_t server(const char *name) {
	return connect(name).pid;
}

} // namespace cock::user::names

#endif // !COCK_USER_NAMES_HPP
