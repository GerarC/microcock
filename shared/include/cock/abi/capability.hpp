/**
 * @file capability.hpp
 * @brief Kinds of capability that the root server can grant to a Ring 3 process.
 */
#ifndef SHARED_COCK_ABI_CAPABILITY_HPP
#define SHARED_COCK_ABI_CAPABILITY_HPP

#include <stdint.h>

namespace cock::abi {

/** Identifier of a node of the derivation tree of capabilities. `SYS_CAP_GRANT` returns one. */
using CapabilityNodeId = uint32_t;

/** Value that names no node. A grant that fails never returns it as an id. */
constexpr CapabilityNodeId NO_CAPABILITY_NODE = 0;

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
	/** Right to exchange messages, synchronous calls and shared memory with the processes whose service label is in the range (see `service.hpp`). */
	IPC = 5,
	/** Right to create processes with `SYS_PROC_CREATE`. It is a range of one (`SPAWN_FIRST`, `SPAWN_COUNT`). */
	SPAWN = 6,
	/** A handle on an endpoint (see `endpoint.hpp`): the range is the endpoint id, a range of one. The number of the node of the derivation tree is the handle that `SYS_EP_SEND` and `SYS_EP_CALL` take. */
	ENDPOINT = 7,
};

/** First valid capability type. */
constexpr CapabilityType FIRST_CAPABILITY_TYPE = CapabilityType::IO_PORTS;

/** Highest valid capability type. */
constexpr CapabilityType HIGHEST_CAPABILITY_TYPE = CapabilityType::ENDPOINT;

/**
 * Bit of a capability type in the mask of `SYS_CAP_ACCEPT`.
 * @param type A capability type.
 * @return One bit, at the position of the number of the type.
 */
constexpr uint32_t capabilityBit(CapabilityType type) {
	return 1u << static_cast<uint32_t>(type);
}

/** Mask with the bit of every valid capability type; `SYS_CAP_ACCEPT` refuses a mask with any other bit. */
constexpr uint32_t ALL_CAPABILITY_TYPES =
	(capabilityBit(HIGHEST_CAPABILITY_TYPE) << 1) - capabilityBit(FIRST_CAPABILITY_TYPE);

/** Mask that accepts no capability in a message, the state of every process until it calls `SYS_CAP_ACCEPT`. */
constexpr uint32_t NO_CAPABILITY_TYPES = 0;

/** Value of the `flags` argument of `SYS_CAP_GRANT` that passes on the right to grant the capability further; zero grants a capability the holder cannot pass on. */
constexpr uint32_t GRANT_DELEGATE = 1;

/** Flag of `SYS_CAP_GRANT` and of a capability message, valid for an `ENDPOINT` capability: the copy may choose the badge of the handles it grants in turn. It passes only when the giver holds the right itself. */
constexpr uint32_t GRANT_MINT = 2;

/** Mask of the flags that `SYS_CAP_GRANT` and a capability message accept; any other bit is refused. */
constexpr uint32_t ALL_GRANT_FLAGS = GRANT_DELEGATE | GRANT_MINT;

/** First (and only) value of the range of a `SPAWN` capability. */
constexpr uint64_t SPAWN_FIRST = 0;

/** Size of the range of a `SPAWN` capability. */
constexpr uint64_t SPAWN_COUNT = 1;

} // namespace cock::abi

#endif // !SHARED_COCK_ABI_CAPABILITY_HPP
