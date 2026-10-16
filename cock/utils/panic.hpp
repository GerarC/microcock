
/** @file
 * @brief Unrecoverable kernel error handler.
 */
#ifndef PANIC_HPP
#define PANIC_HPP

namespace cock::utils {

extern "C" {
/**
 * @brief Reports a fatal kernel error and halts the machine.
 *
 * Prints the panic prefix and the formatted message to the console, then
 * disables interrupts and halts the CPU forever.
 *
 * @param msg printf style format string.
 * @param ... Arguments for the format.
 * @note Never returns. Callable from any context, including interrupt
 * handlers.
 */
__attribute__((noreturn)) void panic(const char *msg, ...);
}

} // namespace cock::utils

#endif // !PANIC_HPP
