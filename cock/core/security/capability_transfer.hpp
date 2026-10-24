/** @file
 * @brief Giving a capability from one process to another and recording it in
 * the derivation tree. The one place where a capability changes hands, used by
 * `SYS_CAP_GRANT` and by capability messages.
 */
#ifndef CORE_SECURITY_CAPABILITY_TRANSFER_HPP
#define CORE_SECURITY_CAPABILITY_TRANSFER_HPP

#include <cock/core/security/capability.hpp>
#include <cock/core/task/process.hpp>
#include <stdint.h>

namespace cock::core::security {

/**
 * @brief Gives part of a capability of one process to another.
 *
 * The giver needs one entry of the type that has the grant flag and contains the
 * whole range. The receiver gets a new entry for the range, recorded in the
 * derivation tree as a child of the entry of the giver, so a holder of an
 * ancestor can revoke it. The entry carries the grant flag only when `delegate`
 * is true.
 *
 * Nothing in this function decides whether the receiver wants the capability or
 * whether the two may talk; the callers check that first.
 *
 * @param giver Process that holds the capability.
 * @param receiver Process that gets the copy.
 * @param type Kind of resource.
 * @param first First resource of the range.
 * @param count Number of resources.
 * @param delegate True to let the receiver pass the copy on.
 * @return The node id of the copy. NO_CAPABILITY_NODE when the giver holds no
 * grantable entry that covers the range (including the none type, a zero count
 * and a range that wraps around), the derivation tree is full or the table of the
 * receiver is full; nothing changes then.
 */
CapabilityNodeId delegate_capability(const task::Process &giver, task::Process &receiver,
									 CapabilityType type, uint64_t first, uint64_t count,
									 bool delegate);

/**
 * @brief Takes back a copy that `delegate_capability` made, when the step that
 * should have followed it failed.
 * @param receiver Process that got the copy.
 * @param node Node id that `delegate_capability` returned.
 * @return True when the entry and the node existed. Nodes derived from the copy
 * move up to its parent, as in `CapabilityTree::detach`.
 */
bool withdraw_capability(task::Process &receiver, CapabilityNodeId node);

} // namespace cock::core::security

#endif // !CORE_SECURITY_CAPABILITY_TRANSFER_HPP
