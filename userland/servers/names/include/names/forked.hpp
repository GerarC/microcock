/** @file
 * @brief The processes that are copies of a program the name server knows (`fork`), and whose rights are those of the
 * program they copy.
 */
#ifndef NAMES_FORKED_HPP
#define NAMES_FORKED_HPP

#include <names/services.hpp>
#include <stddef.h>
#include <stdint.h>

namespace cock::names {

/** Most forked processes the table remembers at once; the slot of one that ended is taken back. */
constexpr size_t MAX_FORKED = 32;

/** Tells whether a process id names a process that lives; `cock::user::process_alive` in the server. */
using AliveCheck = bool (*)(uint32_t);

/**
 * @brief Records that a process is a copy of another one.
 *
 * The copy gets the service of its parent, so it may connect to what the program may connect to and to nothing
 * else. It is not the running process of that service: no label, no endpoint, and the service keeps its own process.
 * A copy of a copy gets the service of the first parent.
 *
 * @param child Process id of the copy; it must live and must not be the running process of a service.
 * @param parent Process id of the program it copies: the running process of a service, or a copy recorded earlier.
 * @param alive How to tell that a process lives.
 * @return true when recorded, or already recorded with the same service. False when the parent is unknown or ended,
 * the child is dead or runs a service, or the table holds `MAX_FORKED` copies that all live.
 */
bool addForked(uint32_t child, uint32_t parent, AliveCheck alive);

/**
 * @brief Finds the service a copy takes its rights from.
 * @param pid Process id.
 * @param alive How to tell that a process lives.
 * @return The service, or null when the process is not a recorded copy or has ended.
 */
Service *findForked(uint32_t pid, AliveCheck alive);

/**
 * @brief Finds the service whose rights a client has: the one it runs, or the one it copies.
 * @param pid Process id of the client.
 * @param alive How to tell that a process lives.
 * @return The service, or null when the client is neither.
 */
Service *findClient(uint32_t pid, AliveCheck alive);

} // namespace cock::names

#endif // !NAMES_FORKED_HPP
