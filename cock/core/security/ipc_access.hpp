/** @file
 * @brief The rule that decides whether one process may talk to another: send a
 * message, make a synchronous call or share memory.
 */
#ifndef CORE_SECURITY_IPC_ACCESS_HPP
#define CORE_SECURITY_IPC_ACCESS_HPP

#include <cock/core/security/capability.hpp>
#include <cock/core/task/process.hpp>
#include <stdint.h>

namespace cock::core::security {

/** @brief Count of a capability check for the label of a single process. */
constexpr uint64_t SINGLE_LABEL = 1;

/**
 * @brief Tells whether `from` may send to, call or share memory with `to`.
 *
 * The right is a channel between two processes, and it is symmetric: it holds when
 * the two are the same process, or when `from` holds an `IPC` capability that covers
 * the service label of `to`, or when `to` holds one that covers the label of `from`.
 * So a client that was given the right to talk to a server also receives the
 * server's answers, and revoking that capability cuts both directions. A process
 * with no label (`NO_SERVICE`) is reached only through a capability that covers
 * label zero, which only the root capability does.
 *
 * The root task holds an `IPC` capability over every label, so every process can
 * reach it through the second rule. This is a known limit until the name service of
 * a later step.
 *
 * Replies to a synchronous call are not checked: the caller started the exchange.
 *
 * @param from Process that sends, calls or shares.
 * @param to Process that receives.
 * @return True when the exchange is allowed. Cost: two scans of a table of
 * `CapabilitySet::MAX_CAPABILITIES` entries at most.
 */
inline bool mayTalk(const task::Process &from, const task::Process &to) {
	if (&from == &to) return true;
	return from.getCapabilities().covers(CapabilityType::IPC, to.getServiceId(), SINGLE_LABEL) ||
		   to.getCapabilities().covers(CapabilityType::IPC, from.getServiceId(), SINGLE_LABEL);
}

} // namespace cock::core::security

#endif // !CORE_SECURITY_IPC_ACCESS_HPP
