/** @file
 * @brief Dispatch of the calls the name server receives.
 */
#ifndef NAMES_REQUESTS_HPP
#define NAMES_REQUESTS_HPP

#include <cock/abi/names.hpp>
#include <stdint.h>

namespace cock::names {

/**
 * @brief Serves one call, which may be one part of a name.
 * @param caller Thread id of the caller, as the kernel stamped it.
 * @param words The request words.
 * @param reply Receives the reply: the status in word 0, and for a successful `CONNECT` the process id and the label of the server.
 * @return true when the call is answered with `reply`; false when it was detached because the caller is not registered yet (see `parked.hpp`): the caller answers it later and sends nothing now.
 */
bool serve(uint32_t caller, const uint32_t (&words)[abi::names::CALL_WORDS],
		   uint32_t (&reply)[abi::names::CALL_WORDS]);

} // namespace cock::names

#endif // !NAMES_REQUESTS_HPP
