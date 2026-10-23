/** @file
 * @brief The registration operations the root server uses to describe the system: `DEFINE`, `ALLOW` and `BIND`.
 */
#ifndef NAMES_REGISTRATION_HPP
#define NAMES_REGISTRATION_HPP

#include <cock/abi/names.hpp>
#include <stdint.h>

namespace cock::names {

/**
 * @brief Serves an `ALLOW` or a `BIND` request.
 *
 * Only the root server may register: any other caller gets `NOT_ROOT` and nothing changes.
 * @param process Process id of the caller (not the thread id), from `thread_process`.
 * @param words The request words.
 * @return `OK` when the table changed; `NOT_ROOT` for a caller that is not `ROOT_PID`; `BAD_REQUEST` for an unknown
 *         label, a service that already lists eight labels, a process id that is not alive or already runs another
 *         service, or an operation that is not `ALLOW` or `BIND`.
 */
abi::names::Status registerService(uint32_t process,
								   const uint32_t (&words)[abi::names::CALL_WORDS]);

/**
 * @brief Serves a `DEFINE` request whose name is complete.
 * @param process Process id of the caller.
 * @param label Service label from the header.
 * @param name The name as it came on the wire; it is stored as a path (short names get `/run/`).
 * @return `OK` when the service was added; `NOT_ROOT` for a caller that is not `ROOT_PID`; `BAD_REQUEST` for a label
 *         out of range or taken, a name taken, or a full table.
 */
abi::names::Status defineProgram(uint32_t process, uint32_t label, const char *name);

} // namespace cock::names

#endif // !NAMES_REGISTRATION_HPP
