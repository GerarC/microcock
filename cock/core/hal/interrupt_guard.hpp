#ifndef CORE_HAL_INTERRUPT_GUARD_HPP
#define CORE_HAL_INTERRUPT_GUARD_HPP

#include <cock/core/hal/utils.hpp>
#include <stdint.h>

namespace cock::core::hal {

class InterruptGuard {
  public:
	InterruptGuard() : flags(save_and_block_interruptions()) {}
	~InterruptGuard() { restore_interruptions(flags); }

	InterruptGuard(const InterruptGuard &) = delete;
	InterruptGuard &operator=(const InterruptGuard &) = delete;

  private:
	uintptr_t flags;
};

} // namespace cock::core::hal

#endif // !CORE_HAL_INTERRUPT_GUARD_HPP
