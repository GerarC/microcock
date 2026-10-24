/** @file
 * @brief Turning a handle into the process that receives and the badge to stamp,
 * for `SYS_EP_SEND` and `SYS_EP_CALL`.
 */
#ifndef CORE_SYSCALL_ENDPOINT_ROUTE_HPP
#define CORE_SYSCALL_ENDPOINT_ROUTE_HPP

#include <cock/core/task/endpoint_table.hpp>
#include <cock/core/task/task_manager.hpp>
#include <cock/core/task/thread.hpp>
#include <stdint.h>

namespace cock::core::syscall {

/** @brief Where a handle leads. */
struct EndpointRoute {
	task::Thread *target = nullptr; /**< Main thread of the process bound to the endpoint; null when the route failed. */
	uint32_t badge = abi::endpoint::NO_BADGE; /**< Badge of the handle. */
};

/**
 * @brief Resolves a handle of the calling thread.
 *
 * The handle is the number of a node of the derivation tree; it works only if the calling
 * process holds an `ENDPOINT` capability with that node, so a number taken from another
 * process, or a revoked one, leads nowhere. Holding the handle is the whole permission: no
 * service label is compared.
 *
 * @param caller Thread that uses the handle.
 * @param handle The number a program passed, as a system call argument.
 * @return The route. `target` is null when the number is not 32 bits wide, names no
 * endpoint capability of the caller, the endpoint is unbound, or the process bound to it
 * has no live main thread.
 */
inline EndpointRoute resolveHandle(const task::Thread &caller, uintptr_t handle) {
	EndpointRoute route;
	if (handle > UINT32_MAX) return route;

	const security::Capability *held =
		caller.getProcess()->getCapabilities().endpointHandle(static_cast<uint32_t>(handle));
	if (held == nullptr) return route;

	uint32_t owner = task::endpoint_table.ownerOf(static_cast<uint32_t>(held->first));
	task::Thread *target = task::TaskManager::getThreadByPID(owner);
	if (owner == 0 || target == nullptr || !target->isMainThread() ||
		target->getState() == task::ThreadState::DEAD)
		return route;

	route.target = target;
	route.badge = held->badge;
	return route;
}

} // namespace cock::core::syscall

#endif // !CORE_SYSCALL_ENDPOINT_ROUTE_HPP
