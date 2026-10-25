/**
 * @file endpoint.hpp
 * @brief Endpoints and handles: the destination of a message is an endpoint the sender holds a handle on, not a process id.
 *
 * An endpoint is a numbered mailbox in the kernel. One process at a time is bound to it and receives what is sent to it;
 * `rs` can bind another process after the first one dies, and the handles that clients hold keep working. The messages
 * and the synchronous calls sent to the endpoint wait in a queue that belongs to the endpoint, so the process bound next
 * receives what was sent while nobody was bound. A handle is an
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
constexpr size_t MAX_ENDPOINTS = 256;

/** Value of `Message::reply` of a message that carries no reply token: every message sent by process id and every message sent without `SEND_EXPECT_REPLY`. */
constexpr uint32_t NO_REPLY = 0;

/** Value of the flags of `SYS_EP_SEND` that asks for nothing extra. */
constexpr uint32_t NO_SEND_FLAGS = 0;

/** Flag of `SYS_EP_SEND`: the receiver may answer with `SYS_EP_REPLY`; the kernel opens a reply token and puts it in `Message::reply`. */
constexpr uint32_t SEND_EXPECT_REPLY = 1;

/** Mask of the flags `SYS_EP_SEND` accepts; any other bit is refused. */
constexpr uint32_t ALL_SEND_FLAGS = SEND_EXPECT_REPLY;

/** Number of reply tokens the kernel can hold open at once, in all the system. */
constexpr size_t MAX_REPLIES = 256;

/** Most reply tokens one process can have open as the one that asked, so a server that never answers cannot use up the table. */
constexpr size_t MAX_REPLIES_PER_REQUESTER = 16;

/** Most asynchronous messages that wait in the queue of one endpoint; a send to a full queue fails like a send to a full inbox. */
constexpr size_t MAX_QUEUED_PER_ENDPOINT = 16;

/** Number of queued messages the kernel can hold for all the endpoints together. */
constexpr size_t QUEUED_MESSAGE_POOL = 256;

/** Id of the first endpoint created; ids follow in order, and a range of the capability that names every endpoint is `FIRST_ENDPOINT` with `MAX_ENDPOINTS` entries. */
constexpr uint32_t FIRST_ENDPOINT = 1;

/** Number of ids a capability that covers every endpoint names. */
constexpr uint64_t ENDPOINT_RANGE_COUNT = MAX_ENDPOINTS;

/** Count of an `ENDPOINT` capability that names one endpoint. */
constexpr uint64_t SINGLE_ENDPOINT = 1;

} // namespace cock::abi::endpoint

#endif // !SHARED_COCK_ABI_ENDPOINT_HPP
