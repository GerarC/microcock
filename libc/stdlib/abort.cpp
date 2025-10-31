#include <stdlib.h>

#if defined(__is_libk)
#include "cock/utils/panic.hpp"
#else
#include <stdio.h>
#endif

#define FOR_ETERNAL for (;;)

using cock::utils::panic;

__attribute__((__noreturn__)) void abort(void) {
#if defined(__is_libk)
	panic("abort()\n");
#else
	// TODO: CREATE a proper kernel panic
	printf("abort()\n");
#endif
	asm volatile("hlt");
	FOR_ETERNAL;
	__builtin_unreachable();
}
