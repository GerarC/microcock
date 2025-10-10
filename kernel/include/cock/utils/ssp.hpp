#ifndef SSP_HPP
#define SSP_HPP

#include <stdint.h>

#if UINT32_MAX == UINTPTR_MAX
#define STACK_CHK_GUARD 0xE2DEE396
#else
#define STACK_CHK_GUARD 0x595E9FBD94FDA766
#endif

extern "C" {
__attribute__((noreturn)) void __stack_chk_fail(void);
}

#endif // !SSP_HPP
