/**
 * @file endpoint.hpp
 * @brief Endpoints and handles: the destination of a message is an endpoint the sender holds a handle on, not a process id.
 *
 * An endpoint is a numbered mailbox in the kernel. One process at a time is bound to it and receives what is sent to it;
 * `init` can bind another process after the first one dies, and the handles that clients hold keep working. A handle is an
 * `ENDPOINT` capability: `SYS_EP_SEND` and `SYS_EP_CALL` take the number of its node of the derivation tree. Every handle
 * carries a badge that the kernel stores in the message, so the receiver learns which client sent it without the sender
 * being able to choose or forge it.
 */
#ifndef SHARED_COCK_ABI_ENDPOINT_HPP
#define SHARED_COCK_ABI_ENDPOINT_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::abi::endpoint {

/** Value that names no endpoint. */
constexpr uint32_t NO_ENDPOINT = 0;

/** Badge of a message that no handle sent: kernel notices and messages sent by process id. */
constexpr uint32_t NO_BADGE = 0;

/** Number of endpoints the kernel can hold. The first one has the id `FIRST_ENDPOINT`. */
constexpr size_t MAX_ENDPOINTS = 64;

/** Id of the first endpoint created; ids follow in order, and a range of the capability that names every endpoint is `FIRST_ENDPOINT` with `MAX_ENDPOINTS` entries. */
constexpr uint32_t FIRST_ENDPOINT = 1;

/** Number of ids a capability that covers every endpoint names. */
constexpr uint64_t ENDPOINT_RANGE_COUNT = MAX_ENDPOINTS;

/** Count of an `ENDPOINT` capability that names one endpoint. */
constexpr uint64_t SINGLE_ENDPOINT = 1;

} // namespace cock::abi::endpoint

#endif // !SHARED_COCK_ABI_ENDPOINT_HPP
