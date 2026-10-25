/** @file
 * @brief The endpoint of each service: one per label, made the first time the program runs and kept
 * across restarts.
 */
#ifndef RS_ENDPOINTS_HPP
#define RS_ENDPOINTS_HPP

#include <cock/ipc/message.hpp>
#include <stdint.h>

namespace cock::rs {

/**
 * @brief Finds the endpoint of a service, making it on first use.
 *
 * The endpoint belongs to the label, not to a process, so a program that restarts receives on the
 * same endpoint and the handles its clients hold keep working.
 *
 * @param label Service label of the program.
 * @return The endpoint id, or `NO_ENDPOINT` when the kernel has no room for another one. A program
 * with no endpoint is still reachable by process id.
 */
uint32_t endpointOf(uint32_t label);

/**
 * @brief Binds the endpoint of a service to the process that runs it.
 *
 * Makes the endpoint if needed. A previous process of the same label stops receiving.
 *
 * @param label Service label of the program.
 * @param pid Process id of the running program.
 * @return The endpoint id the process now receives on, or `NO_ENDPOINT` when there is none or the
 * kernel refused the binding (a process that ended meanwhile).
 */
uint32_t bindEndpoint(uint32_t label, uint32_t pid);

/**
 * @brief Makes a handle for `rs` itself on the endpoint of a service.
 *
 * `rs` holds one capability over every endpoint but that capability is a range, so it cannot send
 * through it. This grants `rs` a capability for the one endpoint, which takes an entry of its table.
 *
 * @param label Service label of the program.
 * @return The handle, or `NO_CAPABILITY_NODE` when the label has no endpoint or the table is full.
 */
uint32_t handleOf(uint32_t label);

/**
 * @brief Sends one asynchronous message to the process that runs a service, through a handle that
 * lives only for the send.
 *
 * Makes a handle with `handleOf` and takes it back with `SYS_CAP_REVOKE` after the send, so the
 * 16 entries of the table of `rs` do not fill up with one handle per program.
 *
 * @param label Service label of the program.
 * @param message The message; the kernel stamps the badge (the process id of `rs`).
 * @return True when the message was queued; false when the label has no endpoint, no process is
 * bound to it, or the inbox of the receiver is full.
 */
bool sendTo(uint32_t label, const cock::ipc::Message &message);

} // namespace cock::rs

#endif // !RS_ENDPOINTS_HPP
