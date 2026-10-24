/** @file
 * @brief The endpoint of each service: one per label, made the first time the program runs and kept
 * across restarts.
 */
#ifndef INIT_ENDPOINTS_HPP
#define INIT_ENDPOINTS_HPP

#include <stdint.h>

namespace cock::init {

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

} // namespace cock::init

#endif // !INIT_ENDPOINTS_HPP
