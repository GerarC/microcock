/**
 * @file capability.hpp
 * @brief Kinds of capability that the root server can grant to a Ring 3 process.
 */
#ifndef SHARED_COCK_ABI_CAPABILITY_HPP
#define SHARED_COCK_ABI_CAPABILITY_HPP

#include <stdint.h>

namespace cock::abi {

/** Kind of resource that a capability gives access to. The kernel checks it before every privileged call. */
enum class CapabilityType : uint8_t {
	/** Empty slot; grants nothing. */
	NONE = 0,
	/** A range of I/O ports (base and count); needed by `SYS_REQUEST_RESOURCE` and port access. */
	IO_PORTS = 1,
	/** A hardware interrupt line that can be bound with `SYS_IRQ_BIND`. */
	IRQ = 2,
	/** A physical address range that can be mapped with `MAP_PHYS` (MMIO). */
	PHYSICAL_MEMORY = 3,
	/** Access to a boot data module opened with `SYS_MODULE_OPEN`. */
	BOOT_MODULE = 4,
};

} // namespace cock::abi

#endif // !SHARED_COCK_ABI_CAPABILITY_HPP
