/** @file
 * @brief Revocation of capabilities: takes a delegated capability, and
 * everything derived from it, away from its holders and undoes what they did
 * with it.
 */
#ifndef CORE_SECURITY_REVOCATION_HPP
#define CORE_SECURITY_REVOCATION_HPP

#include <cock/core/security/capability.hpp>
#include <cock/core/task/process.hpp>

namespace cock::core::security {

/**
 * @brief Revokes a node of the derivation tree and all the nodes derived from it.
 *
 * The caller must hold an ancestor of the node: the process that granted it,
 * a process above that one, or the root task, which holds the root of every
 * chain. A root node (a capability the kernel gave to the first program) has no
 * ancestor, so it cannot be revoked, and nobody can revoke a node they hold
 * themselves.
 *
 * For each node of the subtree the entry leaves the table of its holder and the
 * node leaves the tree. Then, for every value the holder no longer covers
 * through another capability:
 * - I/O ports are closed in its permission map,
 * - interrupt lines it owns are unbound and masked,
 * - device memory it mapped (regions made with `MAP_PHYS`) is unmapped whole,
 *   even when only part of the region lost its capability.
 *
 * A boot module that the holder already opened stays mapped until the holder
 * unmaps it; only opening it again is refused. A holder that has already exited
 * has no node left and nothing to undo.
 *
 * Runs with interrupts off. The cost grows with the number of ports and lines
 * revoked (a check per value), not with the use the holder made of them.
 *
 * @param caller Process that asks for the revocation.
 * @param node Node to revoke.
 * @return True when the node existed and the caller holds an ancestor of it.
 * False otherwise, and nothing changes.
 */
bool revoke_capability(task::Process &caller, CapabilityNodeId node);

} // namespace cock::core::security

#endif // !CORE_SECURITY_REVOCATION_HPP
