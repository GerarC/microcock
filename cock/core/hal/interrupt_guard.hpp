/** @file
 * @brief RAII guard that blocks interrupts for the duration of a scope.
 */
#ifndef CORE_HAL_INTERRUPT_GUARD_HPP
#define CORE_HAL_INTERRUPT_GUARD_HPP

#include <cock/core/hal/utils.hpp>
#include <stdint.h>

namespace cock::core::hal {

/**
 * @brief Disables interrupts on construction and restores the previous state
 * on destruction.
 *
 * Nesting is safe: the saved flags are restored, so an inner guard never
 * enables interrupts that an outer one disabled. It is the kernel critical
 * section primitive on a single CPU. It is not copyable.
 */
class InterruptGuard {
  public:
	/** @brief Saves the interrupt state and blocks interrupts. */
	InterruptGuard() : flags(save_and_block_interruptions()) {}
	/** @brief Restores the interrupt state saved by the constructor. */
	~InterruptGuard() { restore_interruptions(flags); }

	/** @brief Copying is forbidden: a guard owns one saved state. */
	InterruptGuard(const InterruptGuard &) = delete;
	/** @brief Assignment is forbidden: a guard owns one saved state. */
	InterruptGuard &operator=(const InterruptGuard &) = delete;

  private:
	uintptr_t flags; /**< CPU flags saved when the guard was created. */
};

} // namespace cock::core::hal

#endif // !CORE_HAL_INTERRUPT_GUARD_HPP
