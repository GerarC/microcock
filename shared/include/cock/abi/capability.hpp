#ifndef SHARED_COCK_ABI_CAPABILITY_HPP
#define SHARED_COCK_ABI_CAPABILITY_HPP

#include <stdint.h>

namespace cock::abi {

enum class CapabilityType : uint8_t {
	NONE = 0,
	IO_PORTS = 1,
	IRQ = 2,
	PHYSICAL_MEMORY = 3,
	BOOT_MODULE = 4,
};

} // namespace cock::abi

#endif // !SHARED_COCK_ABI_CAPABILITY_HPP
