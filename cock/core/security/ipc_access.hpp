/** @file
 * @brief The rule that decides whether one process may share memory with another.
 */
#ifndef CORE_SECURITY_IPC_ACCESS_HPP
#define CORE_SECURITY_IPC_ACCESS_HPP

#include <cock/core/security/capability.hpp>
#include <cock/core/task/endpoint_table.hpp>
#include <cock/core/task/process.hpp>
#include <stdint.h>

namespace cock::core::security {

/**
 * @brief Tells whether `holder` has a handle on an endpoint that `target` receives on.
 *
 * @param holder Process whose capabilities are searched.
 * @param target Process that may be bound to the endpoint of one of them.
 * @return True when an `ENDPOINT` capability of `holder` names an endpoint bound to `target`.
 * Cost: one scan of a table of `CapabilitySet::MAX_CAPABILITIES` entries.
 */
inline bool holdsHandleTo(const task::Process &holder, const task::Process &target) {
	return holder.getCapabilities().anyOf(CapabilityType::ENDPOINT, [&](const Capability &entry) {
		return task::endpoint_table.ownerOf(static_cast<uint32_t>(entry.first)) == target.getId();
	});
}

/**
 * @brief Tells whether `from` may share memory with `to`.
 *
 * The right is a channel between two processes, and it is symmetric: it holds when the two are
 * the same process, or when `from` holds a handle on an endpoint that `to` receives on, or when
 * `to` holds one on an endpoint that `from` receives on. So a client that was given a handle on
 * a server may hand it a window, and revoking the handle cuts both directions. A message is not
 * covered here: it goes through a handle (`SYS_EP_SEND`), and the handle is the whole permission.
 *
 * @param from Process that shares.
 * @param to Process that receives the object.
 * @return True when the exchange is allowed. Cost: two scans of a table of
 * `CapabilitySet::MAX_CAPABILITIES` entries at most.
 */
inline bool mayShare(const task::Process &from, const task::Process &to) {
	if (&from == &to) return true;
	return holdsHandleTo(from, to) || holdsHandleTo(to, from);
}

} // namespace cock::core::security

#endif // !CORE_SECURITY_IPC_ACCESS_HPP
