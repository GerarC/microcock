#include "cock/utils/panic.hpp"
#include <stdio.h>
#include <stdlib.h>

#define ETERNAL ;;

using cock::utils::panic;

__attribute__((__noreturn__)) void abort(void) {
#if defined(__is_libk)
	panic("abort()\n");
#else
	// TODO: CREATE a proper kernel panic
	printf("abort()\n");
#endif
	asm volatile("hlt");
	for (ETERNAL);
	__builtin_unreachable();
}
