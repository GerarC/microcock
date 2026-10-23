/** @file
 * @brief The registration operations the root server uses to describe the system: `DEFINE`, `ALLOW` and `BIND`.
 */
#ifndef NAMES_REGISTRATION_HPP
#define NAMES_REGISTRATION_HPP

#include <cock/abi/names.hpp>
#include <stdint.h>

namespace cock::names {

/**
 * @brief Serves a `DEFINE`, `ALLOW` or `BIND` request.
 *
 * Only the root server may register: any other caller gets `NOT_ROOT` and nothing changes.
 * @param caller Process id of the caller, as the kernel stamped it.
 * @param words The request words.
 * @return `OK` when the table changed; `NOT_ROOT` for a caller that is not `ROOT_PID`; `BAD_REQUEST` for a label
 *         out of range, a duplicate label or name, an unknown label in `ALLOW` or `BIND`, a service that already
 *         lists eight labels, a table that is full, a process id that is not alive or already runs another
 *         service, or an operation that is not a registration.
 */
abi::names::Status registerService(uint32_t caller,
								   const uint32_t (&words)[abi::names::CALL_WORDS]);

} // namespace cock::names

#endif // !NAMES_REGISTRATION_HPP
