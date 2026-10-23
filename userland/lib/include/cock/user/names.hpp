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
 * in the middle of a program that serves requests. Only the first thread of a process may connect: the name
 * server knows processes, and a call carries the id of the calling thread.
 */
#ifndef COCK_USER_NAMES_HPP
#define COCK_USER_NAMES_HPP

#include <cock/abi/names.hpp>
#include <cock/abi/service.hpp>
#include <cock/user/api.hpp>
#include <cock/user/call.hpp>
#include <stdint.h>

namespace cock::user::names {

/** Times `connect` asks again when the name server does not know the caller yet. */
constexpr uint32_t UNKNOWN_CLIENT_RETRIES = 5;
/** Pause between those attempts, in milliseconds (rounded up to a clock tick by the kernel). */
constexpr uint32_t UNKNOWN_CLIENT_PAUSE_MS = 1;

/** Answer of the name server to a connect request. */
struct Connection {
	/** Result code; the other fields are valid only for `OK`. */
	abi::names::Status status;
	/** Process id of the server. */
	uint32_t pid;
	/** Service label of the server, which covers every process that runs the same program. */
	uint32_t label;
};

/**
 * @brief Asks the name server for the right to talk to a server.
 *
 * Blocks until the name server answers. Right after a program starts, `init` may not have registered it yet;
 * the call then asks again up to `UNKNOWN_CLIENT_RETRIES` times.
 * @param name NUL terminated name of the server, 1 to `abi::names::MAX_NAME_BYTES` bytes.
 * @return `OK` with the process id and label; `NOT_FOUND` when no running program has the name;
 *         `DENIED` when the manifest does not let the caller talk to it; `UNKNOWN_CLIENT` when the caller is not
 *         a registered process or not the first thread of one; `NO_ROOM` when the capability could not be
 *         given; `BAD_REQUEST` for an empty or too long name or when the name server could not be called. In
 *         every other case than `OK` the process id is `NO_PROCESS`.
 */
inline Connection connect(const char *name) {
	Connection connection = {abi::names::Status::BAD_REQUEST, NO_PROCESS, abi::service::NO_SERVICE};

	uint32_t words[abi::names::CALL_WORDS] = {abi::names::pack(abi::names::Op::CONNECT)};
	if (!abi::names::packName(name, words)) return connection;

	for (uint32_t attempt = 0; attempt <= UNKNOWN_CLIENT_RETRIES; attempt++) {
		CallResult result = ipc_call(abi::service::NAMES_PID, words);
		if (!result.ok) return connection;

		connection.status = static_cast<abi::names::Status>(result.words[abi::names::HEADER_WORD]);
		if (connection.status == abi::names::Status::OK) {
			connection.pid = result.words[abi::names::PID_WORD];
			connection.label = result.words[abi::names::LABEL_WORD];
			return connection;
		}
		if (connection.status != abi::names::Status::UNKNOWN_CLIENT) return connection;
		sleep_ms(UNKNOWN_CLIENT_PAUSE_MS);
	}
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
