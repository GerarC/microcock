#ifndef ARCH_X86_HAL_PROCESS_CONTEXT_HPP
#define ARCH_X86_HAL_PROCESS_CONTEXT_HPP

/**
 * @file
 * @brief HAL: architecture specific part of a process descriptor.
 *
 * Port requirement: define ArchProcessContext (default constructible, trivially
 * destructible by the core, which releases its resources through the port) and
 * IO_PERMISSION_MAP_BYTES. A port without port mapped I/O can leave the struct
 * empty. The core embeds one ArchProcessContext per process without looking
 * inside it.
 */

#include <stddef.h>
#include <stdint.h>

namespace cock::core::hal {

/** @brief Bytes of the TSS I/O permission bitmap: 65536 ports at one bit each
 * (8192 bytes) plus the mandatory terminating 0xFF byte. */
constexpr size_t IO_PERMISSION_MAP_BYTES = 8193;

/**
 * @brief Per process x86 state: the I/O port grants.
 *
 * Owned by the process; the bitmap is allocated on the first port grant and
 * freed when the process is destroyed.
 */
struct ArchProcessContext {
	/** @brief I/O permission bitmap in TSS format (bit clear means allowed), or
	 * null while the process holds no port. */
	uint8_t *iopm = nullptr;
	/** @brief Number of leading bytes of #iopm that carry a grant; zero keeps
	 * the whole map disabled. */
	uint32_t iopm_bytes = 0;
	/** @brief Version stamp bumped on every change, so the context switch can
	 * skip copying an unchanged bitmap into the TSS. */
	uint32_t version = 0;
};

} // namespace cock::core::hal

#endif // !ARCH_X86_HAL_PROCESS_CONTEXT_HPP
