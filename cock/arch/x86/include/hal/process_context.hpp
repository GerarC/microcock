#ifndef ARCH_X86_HAL_PROCESS_CONTEXT_HPP
#define ARCH_X86_HAL_PROCESS_CONTEXT_HPP

#include <stddef.h>
#include <stdint.h>

namespace cock::core::hal {

constexpr size_t IO_PERMISSION_MAP_BYTES = 8193;

struct ArchProcessContext {
	uint8_t *iopm = nullptr;
	uint32_t iopm_bytes = 0;
	uint32_t version = 0;
};

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_PROCESS_CONTEXT_HPP
