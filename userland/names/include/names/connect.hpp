/** @file
 * @brief The `CONNECT` operation: check the policy and give the client a handle on a server.
 */
#ifndef NAMES_CONNECT_HPP
#define NAMES_CONNECT_HPP

#include <cock/abi/names.hpp>
#include <stdint.h>

namespace cock::names {

/**
 * @brief Serves a `CONNECT` request.
 *
 * Finds the client by its process id (the badge of the handle the call came through, which the kernel wrote, so
 * a client cannot lie about it), finds the server by path, applies `mayConnect` and gives the client, once, a handle on the endpoint of the server whose badge is the process id of the client.
 * @param client Process id of the caller.
 * @param name NUL terminated path of the server, already normalized.
 * @param reply Receives the reply words; the process id, the label and the handle are set only on `OK`.
 * @return The status: `UNKNOWN_CLIENT` when the caller is not bound to a service, `NOT_FOUND` when no service
 *         has the name or its process ended or is not bound to an endpoint yet, `DENIED` when the policy refuses,
 *         `NO_ROOM` when the grant failed, `OK` otherwise.
 */
abi::names::Status connect(uint32_t client, const char *name,
						   uint32_t (&reply)[abi::names::CALL_WORDS]);

} // namespace cock::names

#endif // !NAMES_CONNECT_HPP
