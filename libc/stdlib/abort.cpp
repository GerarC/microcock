#include <stdio.h>
#include <stdlib.h>

#define ETERNAL ;;

__attribute__((__noreturn__)) void abort(void) {
#if defined(__is_libk)
	// TODO: CREATE a proper kernel panic
	printf("kernel: panic: abort()\n");
	asm volatile("hlt");
#else
	// TODO: CREATE a proper kernel pani
	printf("abort()\n");
#endif
	for (ETERNAL);
	__builtin_unreachable();
}
