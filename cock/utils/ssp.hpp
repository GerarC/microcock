/** @file
 * @brief Stack smashing protection support: the canary value and the failure
 * handler the compiler calls.
 */
#ifndef SSP_HPP
#define SSP_HPP

#include <stdint.h>


/** @brief Initial value of the stack canary, sized for the pointer width of
 * the target. */
#if UINT32_MAX == UINTPTR_MAX
#define STACK_CHK_GUARD 0xE2DEE396
#else
#define STACK_CHK_GUARD 0x595E9FBD94FDA766
#endif

extern "C" {
/**
 * @brief Called by compiler generated code when a stack canary was overwritten.
 *
 * Panics in the kernel, aborts in a hosted build, otherwise halts.
 * @note Never returns.
 */
__attribute__((noreturn)) void __stack_chk_fail(void);
}

#endif // !SSP_HPP
