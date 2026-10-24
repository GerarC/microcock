/** @file
 * @brief The record of the `IPC` capabilities the name server already gave, so a client that connects
 * again does not use another slot of its capability table and another node of the derivation tree.
 */
#ifndef NAMES_GRANTS_HPP
#define NAMES_GRANTS_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::names {

/** Most grants kept; the derivation tree of the kernel holds 256 nodes in all. */
constexpr size_t MAX_GRANTS = 128;

/**
 * @brief Tells whether a process already holds the capability for a label.
 * @param client Process id of the client.
 * @param label Service label of the server.
 * @return true when `record` stored the pair and the client is still alive.
 */
bool granted(uint32_t client, uint32_t label);

/**
 * @brief Finds the endpoint handle a client holds for a label.
 * @param client Process id of the client.
 * @param label Service label of the server.
 * @return The handle `record` stored, or `NO_CAPABILITY_NODE` when the pair is unknown or the server
 *         had no endpoint when the client connected.
 */
uint32_t handleOf(uint32_t client, uint32_t label);

/**
 * @brief Makes room for one more grant.
 *
 * When the table is full it drops the entries of processes that ended (`process_alive`).
 * @return true when a slot is free; false when every entry belongs to a live process.
 */
bool hasRoom();

/**
 * @brief Remembers that a client holds the capability for a label, and the handle it was given.
 * @param client Process id of the client.
 * @param label Service label of the server.
 * @param handle Handle on the endpoint of the server, or `NO_CAPABILITY_NODE` when it has none.
 * @pre `hasRoom()` returned true.
 */
void record(uint32_t client, uint32_t label, uint32_t handle);

} // namespace cock::names

#endif // !NAMES_GRANTS_HPP
