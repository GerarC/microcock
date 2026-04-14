#ifndef CORE_HAL_UTILS_HPP
#define CORE_HAL_UTILS_HPP

namespace cock::core::hal {

/**
 * @brief Halts the CPU until the next interrupt arrives.
 * * Reduces power consumption and CPU usage when the system scheduler is idle.
 */
void halt();

/**
 * @brief Disables hardware interrupts globally for the current CPU.
 * * Used to protect critical sections of code from race conditions.
 */
void block_interruptions();

/**
 * @brief Re-enables hardware interrupts globally for the current CPU.
 */
void unblock_interruptions();

/**
 * @brief Manually triggers a timer interrupt (IRQ0) via a software interrupt
 * vector.
 * * Useful for forcing a scheduler context switch explicitly.
 */
void manual_timer();

} // namespace cock::core::hal

#endif // !CORE_HAL_UTILS_HPP
